#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <czmq.h>
#include <ws.h>

void *context;
void *pub_socket;

void *forward_messages(void *arg) {
    int fd =  *((int *) arg);
    free(arg);
    void *sub_context = zmq_ctx_new();
    void *sub_socket = zmq_socket(sub_context, ZMQ_SUB);
    zmq_connect(sub_socket, "tcp://localhost:5560");
    zmq_setsockopt(sub_socket, ZMQ_SUBSCRIBE, "LIDAR", 5);
    char buffer[64];
    char *cli;
    cli = ws_getaddress(fd);
    printf("Starting subscriber, client: %d | addr: %s\n", fd, cli);
    free(cli);
    while (1) {
        if (zmq_recv(sub_socket, buffer, sizeof(buffer), 0) > 0) {
            float angle, distance;
            int quality;
            if (sscanf(buffer, "LIDAR %f %f %d", &angle, &distance, &quality)) {
                sprintf(buffer, "[{\"angle\": %f, \"distance\": %f}]", angle, distance);
                ws_sendframe_txt(fd, buffer, false);
            }
        }
    }
}

/**
 * @brief This function is called whenever a new connection is opened.
 * @param fd The new client file descriptor.
 */
void onopen(int fd)
{
    char *cli;
    cli = ws_getaddress(fd);
    printf("Connection opened, client: %d | addr: %s\n", fd, cli);
    free(cli);
    pthread_t thread;
    int *arg = malloc(sizeof(*arg));
    *arg = fd;
    pthread_create(&thread, NULL, forward_messages, arg);
}

/**
 * @brief This function is called whenever a connection is closed.
 * @param fd The client file descriptor.
 */
void onclose(int fd)
{
    char *cli;
    cli = ws_getaddress(fd);
    printf("Connection closed, client: %d | addr: %s\n", fd, cli);
    free(cli);
}

/**
 * @brief Message events goes here.
 * @param fd   Client file descriptor.
 * @param msg  Message content.
 * @param size Message size.
 * @param type Message type.
 */
void onmessage(int fd, const unsigned char *msg, uint64_t size, int type)
{
    zmq_send(pub_socket, msg, size, 0);
}

int main()
{
    /* Register events. */
    struct ws_events evs;
    evs.onopen    = &onopen;
    evs.onclose   = &onclose;
    evs.onmessage = &onmessage;

    context = zmq_ctx_new ();
    pub_socket = zmq_socket (context, ZMQ_PUB);
    zmq_connect (pub_socket, "tcp://localhost:5559");

    /*
     * Main loop, this function never* returns.
     *
     * *If the third argument is != 0, a new thread is created
     * to handle new connections.
     */
    ws_socket(&evs, 8080, 0);

    return (0);
}