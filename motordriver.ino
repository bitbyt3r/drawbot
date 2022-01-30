#include <Adafruit_MotorShield.h>
#include <Servo.h>
#include <util/crc16.h>
#include <avr/io.h>
#include <avr/interrupt.h>

Adafruit_MotorShield AFMS = Adafruit_MotorShield();
Adafruit_DCMotor *fr = AFMS.getMotor(1);
Adafruit_DCMotor *fl = AFMS.getMotor(2);
Adafruit_DCMotor *rl = AFMS.getMotor(3);
Adafruit_DCMotor *rr = AFMS.getMotor(4);
Servo servo;

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
  uint16_t speed;
  uint8_t dir;
  Adafruit_DCMotor *motor;
} MotorState;

PosMessage pos;
SpeedMessage msg;
MotorState motor_state[4];
byte enc_state;

void updateMotor(int16_t velocity, uint8_t idx) {
  MotorState *state = &motor_state[idx];
  byte dir = RELEASE;
  uint16_t speed = abs(velocity);
  if (velocity > 0) {
    dir = FORWARD;
  } else if (velocity < 0) {
    dir = BACKWARD;
  }
  if (dir != state->dir) {
    state->motor->run(dir);
  }
  if (speed != state->speed) {
    state->motor->setSpeedFine(speed);
  }
  state->dir = dir;
  state->speed = speed;
}

void updateMotors(SpeedMessage *msg) {
  updateMotor(msg->fr_vel, 0);
  updateMotor(msg->fl_vel, 1);
  updateMotor(msg->rl_vel, 2);
  updateMotor(msg->rr_vel, 3);
  servo.write(msg->servo_pos);
}

uint16_t getCRC(SpeedMessage *x) {
  uint16_t crc = 0;
  crc = _crc16_update(crc, x->fr_vel);
  crc = _crc16_update(crc, x->fl_vel);
  crc = _crc16_update(crc, x->rl_vel);
  crc = _crc16_update(crc, x->rr_vel);
  crc = _crc16_update(crc, x->servo_pos);
  return crc;
}

bool checkCRC(SpeedMessage *x) {
  uint16_t crc = 0;
  crc = _crc16_update(crc, x->fr_vel);
  crc = _crc16_update(crc, x->fl_vel);
  crc = _crc16_update(crc, x->rl_vel);
  crc = _crc16_update(crc, x->rr_vel);
  crc = _crc16_update(crc, x->servo_pos);
  return crc == x->crc;
}

void setCRC(PosMessage *x) {
  uint16_t crc = 0;
  crc = _crc16_update(crc, x->fr_pos);
  crc = _crc16_update(crc, x->fl_pos);
  crc = _crc16_update(crc, x->rl_pos);
  crc = _crc16_update(crc, x->rr_pos);
  x->crc = crc;
}

ISR(PCINT2_vect) {
  byte new_state = PINK;
  byte changed = enc_state ^ new_state;
  enc_state = new_state;
  byte statePinA = new_state & 0x55;
  byte statePinB = (new_state>>1) & 0x55;
  byte incr = changed & (statePinA ^ statePinB);
  byte decr = (changed>>1) & (statePinA ^ statePinB);
  if (incr & _BV(2)) {
    pos.fr_pos++;
  } else if (decr & _BV(2)) {
    pos.fr_pos--;
  }
  if (incr & _BV(6)) {
    pos.fl_pos--;
  } else if (decr & _BV(6)) {
    pos.fl_pos++;
  }
  if (incr & _BV(4)) {
    pos.rl_pos--;
  } else if (decr & _BV(4)) {
    pos.rl_pos++;
  }
  if (incr & _BV(0)) {
    pos.rr_pos++;
  } else if (decr & _BV(0)) {
    pos.rr_pos--;
  }
}

uint8_t buffer[sizeof(msg)];
uint8_t buffer_start;
uint8_t buffer_size;

void setup() {
  DDRK = 0;
  PORTK = 0x00;
  pos.fr_pos = 0;
  pos.fl_pos = 0;
  pos.rl_pos = 0;
  pos.rr_pos = 0;
  for (int i=0; i<4; i++) {
    motor_state[i].speed = 0;
    motor_state[i].dir = RELEASE;
  }
  motor_state[0].motor = fr;
  motor_state[1].motor = fl;
  motor_state[2].motor = rl;
  motor_state[3].motor = rr;
  msg.fr_vel = 0;
  msg.fl_vel = 0;
  msg.rl_vel = 0;
  msg.rr_vel = 0;
  msg.servo_pos = 0;
  msg.crc = 0;
  servo.attach(10);
  cli();
  enc_state = 0;
  PCICR |= _BV(PCIE2);
  PCMSK2 = 0xFF;
  sei();
  buffer_start = 0;
  buffer_size = 0;
  Serial.begin(115200);
}

void loop() {
  if (Serial.available()) {
    uint8_t idx = (buffer_start + buffer_size) % sizeof(msg);
    buffer[idx] = Serial.read();
    if (buffer_size < sizeof(msg)) {
      buffer_size++;
    } else {
      buffer_start++;
    }
    for (int i=0; i<sizeof(msg); i++) {
      ((byte*)&msg)[i] = buffer[(i+buffer_start)%sizeof(msg)];
    }    
    if (checkCRC(&msg)) {
      updateMotors(&msg);
    }
  }
  PosMessage tempPos;
  cli();
  memcpy(&tempPos, &pos, sizeof(pos));
  sei();
  setCRC(&tempPos);
  Serial.write((byte*)&tempPos, sizeof(tempPos));
}