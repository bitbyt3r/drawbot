#include <errno.h>
#include <fcntl.h> 
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <czmq.h>
#include <zmq.h>
#include <signal.h>
#include "serial.c"

static volatile int keepRunning = 1;

void intHandler(int dummy) {
    keepRunning = 0;
}

typedef struct {
  int16_t fr_vel;
  int16_t fl_vel;
  int16_t rl_vel;
  int16_t rr_vel;
  uint16_t servo_pos;
  uint16_t crc;
} SpeedMessage;

typedef struct {
  int16_t fr_pos;
  int16_t fl_pos;
  int16_t rl_pos;
  int16_t rr_pos;
  uint16_t crc;
} PosMessage;

typedef struct {
    int64_t fr_pos;
    int64_t fl_pos;
    int64_t rl_pos;
    int64_t rr_pos;
} Position;

uint16_t crc16_update(uint16_t crc, uint8_t a) {
    int i;
    crc ^= a;
    for (i = 0; i < 8; ++i)
    {
        if (crc & 1)
        crc = (crc >> 1) ^ 0xA001;
        else
        crc = (crc >> 1);
    }
    return crc;
}

uint16_t swap_int16( uint16_t val ) {
    return (val << 8) | ((val >> 8) & 0xFF);
}

void swap_endianness(SpeedMessage *msg) {
    msg->fr_vel = swap_int16(msg->fr_vel);
    msg->fl_vel = swap_int16(msg->fl_vel);
    msg->rl_vel = swap_int16(msg->rl_vel);
    msg->rr_vel = swap_int16(msg->rr_vel);
    msg->servo_pos = swap_int16(msg->servo_pos);
    msg->crc = swap_int16(msg->crc);
}

void updateCRC(SpeedMessage *msg) {
    uint16_t crc = 0;
    crc = crc16_update(crc, msg->fr_vel);
    crc = crc16_update(crc, msg->fl_vel);
    crc = crc16_update(crc, msg->rl_vel);
    crc = crc16_update(crc, msg->rr_vel);
    crc = crc16_update(crc, msg->servo_pos);
    msg->crc = crc;
}

bool checkCRC(PosMessage *pos) {
    uint16_t crc = 0;
    crc = crc16_update(crc, pos->fr_pos);
    crc = crc16_update(crc, pos->fl_pos);
    crc = crc16_update(crc, pos->rl_pos);
    crc = crc16_update(crc, pos->rr_pos);
    return crc == pos->crc;
}

int64_t unwrap_int(int16_t new, int16_t last) {
    int64_t dist = 0;
    if ((new > (INT16_MAX/2)) & (last < (INT16_MAX/-2))) {
        dist = (new - last) - INT16_MAX*2;
    } else if ((last > (INT16_MAX/2)) & (new < (INT16_MAX/-2))) {
        dist = INT16_MAX*2 - (last - new);
    } else {
        dist = new - last;
    }
    return dist;
}

int main() {
    signal(SIGINT, intHandler);

    void *context = zmq_ctx_new ();
    void *pub_socket = zmq_socket (context, ZMQ_PUB);
    zmq_connect (pub_socket, "tcp://localhost:5559");
    void *sub_socket = zmq_socket(context, ZMQ_SUB);
    zmq_connect(sub_socket, "tcp://localhost:5560");
    if (zmq_setsockopt(sub_socket, ZMQ_SUBSCRIBE, "DRIVE", 5)) {
        printf("Failed to setsockopt %d %s\n", errno, zmq_strerror(errno));
    }

    printf("Connected\n");

    char *portname = "/dev/ttyACM0";
    char buf[sizeof(PosMessage)];
    uint8_t buf_start = 0;

    int fd = open(portname, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        printf ("error %d opening %s: %s", errno, portname, strerror (errno));
        return -1;
    }

    set_interface_attribs (fd, B115200, 0);  // set speed to 115,200 bps, 8n1 (no parity)
    set_blocking (fd, 0);                // set no blocking

    SpeedMessage msg;
    PosMessage last_pos;
    last_pos.fr_pos = 0;
    last_pos.fl_pos = 0;
    last_pos.rl_pos = 0;
    last_pos.rr_pos = 0;
    Position current_pos;
    current_pos.fr_pos = 0;
    current_pos.fl_pos = 0;
    current_pos.rl_pos = 0;
    current_pos.rr_pos = 0;

    while (keepRunning) {
        int n = read(fd, buf+buf_start, 1);
        buf_start = buf_start + n;
        buf_start = buf_start % sizeof(PosMessage);
        if (buf_start == 0) {
            PosMessage pos;
            memcpy(&pos, &buf, sizeof(pos));
            if (checkCRC(&pos)) {
                current_pos.fr_pos = current_pos.fr_pos + unwrap_int(pos.fr_pos, last_pos.fr_pos);
                current_pos.fl_pos = current_pos.fl_pos + unwrap_int(pos.fl_pos, last_pos.fl_pos);
                current_pos.rl_pos = current_pos.rl_pos + unwrap_int(pos.rl_pos, last_pos.rl_pos);
                current_pos.rr_pos = current_pos.rr_pos + unwrap_int(pos.rr_pos, last_pos.rr_pos);
                memcpy(&last_pos, &pos, sizeof(pos));
                char msg[128];
                int msg_len = sprintf(msg, "WHEEL_ENCODER %lld %lld %lld %lld", current_pos.fr_pos, current_pos.fl_pos, current_pos.rl_pos, current_pos.rr_pos);
                zmq_send (pub_socket, msg, msg_len, 0);
            } else {
                memmove(buf, buf+1, sizeof(buf)-1);
                buf_start = sizeof(buf)-1;
            }
        }

        char buffer [64];
        if (zmq_recv (sub_socket, buffer, sizeof(buffer), ZMQ_DONTWAIT) > 0) {
            int m1, m2, m3, m4, servo;
            if (sscanf(buffer, "DRIVE %d %d %d %d %d", &m1, &m2, &m3, &m4, &servo)) {
                msg.fr_vel = m1;
                msg.fl_vel = m2;
                msg.rl_vel = m3;
                msg.rr_vel = m4;
                msg.servo_pos = servo;
                updateCRC(&msg);
                write(fd, &msg, sizeof(msg));
            }
        }
    }

    printf("Shutting down...\n");
    msg.fr_vel = 0;
    msg.fl_vel = 0;
    msg.rl_vel = 0;
    msg.rr_vel = 0;
    msg.servo_pos = 0;
    updateCRC(&msg);
    write(fd, &msg, sizeof(msg));
    close(fd);
    zmq_close (pub_socket);
    zmq_close (sub_socket);
    zmq_ctx_destroy (context);
    return 0;
}
