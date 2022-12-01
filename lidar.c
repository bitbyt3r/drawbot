#include <stdio.h>
#include <pigpio.h>
#include <signal.h>
#include <unistd.h>
#include <czmq.h>
#include <math.h>
#include "serial.c"

#define MOTOR_A 13
#define MOTOR_B 12
#define SLEEP 16
#define FAULT 20
#define SENTINEL 0xFA

static volatile sig_atomic_t keepRunning = 1;

void intHandler(int dummy) {
    keepRunning = 0;
    printf("Shutting down\n");
    gpioWrite(SLEEP, 0);
    gpioPWM(MOTOR_B, 0);
    gpioTerminate();
    exit(0);
}

typedef struct {
    uint16_t distance;
    uint16_t quality;
} Point;

typedef struct {
    uint8_t sentinel;
    uint8_t angle;
    uint16_t speed;
    Point points[4];
    uint16_t checksum;
} LidarPacket;

int main(int argc, char *argv[]) {
    if (gpioInitialise() < 0) {
        fprintf(stderr, "pigpio initialisation failed\n");
        return 1;
    }
    signal(SIGINT, intHandler);
    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = intHandler;
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGCONT, &action, NULL);
    sigaction(SIGHUP, &action, NULL);

    gpioSetMode(MOTOR_A, PI_OUTPUT);
    gpioSetMode(MOTOR_B, PI_OUTPUT);
    gpioSetMode(SLEEP, PI_OUTPUT);
    gpioSetMode(FAULT, PI_INPUT);

    gpioWrite(MOTOR_A, 0);
    gpioWrite(SLEEP, 1);
    gpioPWM(MOTOR_B, 220);

    void *context = zmq_ctx_new ();
    void *requester = zmq_socket (context, ZMQ_PUB);
    zmq_connect (requester, "tcp://localhost:5559");
    printf("Connected\n");
    
    char *portname = "/dev/ttyS0";

    int fd = open(portname, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        printf ("error %d opening %s: %s", errno, portname, strerror (errno));
        return -1;
    }

    set_interface_attribs (fd, B115200, 0);  // set speed to 115,200 bps, 8n1 (no parity)
    set_blocking (fd, 0);  

    char buf[22];
    uint8_t idx = 0;
    LidarPacket packet;
    while (keepRunning) {
        while (keepRunning && buf[0] != SENTINEL) {
            read(fd, buf, 1);
        }
        idx++;
        while (keepRunning && idx < 22) {
            int n = read(fd, buf+idx, 22-idx);
            idx = idx + n;
        }
        if (buf[1] > 0xF9 || buf[1] < 0xA0) {
            idx = 0;
            continue;
        }
        uint32_t checksum = 0;
        for (int i=0; i<10; i++) {
            uint16_t val = buf[i*2] + ((uint16_t)buf[i*2+1]<<8);
            checksum = (checksum << 1) + val;
        }
        checksum = ((checksum & 0x7FFF) + (checksum >> 15)) & 0x7FFF;
        uint16_t msg_checksum = buf[20] + ((uint16_t)buf[21]<<8);
        if (checksum != msg_checksum) {
            printf("Checksum failed, calc %d != msg %d\n", checksum, msg_checksum);
        } else {
            memcpy(&packet, &buf, sizeof(packet));
            for (int i=0; i<4; i++) {
                if (packet.points[i].distance & 0x80) {
                    continue;
                }
                float angle = ((packet.angle - 160) * 4 + i) / 180.0 * M_PI;
                uint16_t dist = packet.points[i].distance & 0x3FFF;
                char msg[64];
                int msg_len = sprintf(msg, "LIDAR %f %d %d", angle, dist, packet.points[i].quality);
                zmq_send (requester, msg, msg_len, 0);
            }
        }
        idx = 0;
        buf[0] = 0;
    }
    return 0;
}
