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

int main() {
    signal(SIGINT, intHandler);

    void *context = zmq_ctx_new ();
    void *requester = zmq_socket (context, ZMQ_PUB);
    zmq_connect (requester, "tcp://localhost:5559");
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
    msg.fr_vel = 0;
    msg.fl_vel = 0;
    msg.rl_vel = 0;
    msg.rr_vel = 0;
    msg.servo_pos = 0;
    updateCRC(&msg);

    while (keepRunning) {
        int n = read(fd, buf+buf_start, 1);
        buf_start = buf_start + n;
        buf_start = buf_start % sizeof(PosMessage);
        if (buf_start == 0) {
            PosMessage pos;
            memcpy(&pos, &buf, sizeof(pos));
            if (checkCRC(&pos)) {
                char msg[64];
                int msg_len = sprintf(msg, "WHEEL_ENCODER %d %d %d %d", pos.fr_pos, pos.fl_pos, pos.rl_pos, pos.rr_pos);
                zmq_send (requester, msg, msg_len, 0);
            } else {
                buf_start++;
            }
        }
    }

    //write (fd, &msg, sizeof(msg));
    printf("Shutting down...\n");
    close(fd);
    zmq_close (requester);
    zmq_ctx_destroy (context);
    return 0;
}
