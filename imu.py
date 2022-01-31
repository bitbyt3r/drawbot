#!/usr/bin/python3
import adafruit_bno055
import board
import time
import zmq

i2c = board.I2C()

sensor = adafruit_bno055.BNO055_I2C(i2c)

context = zmq.Context()
socket = context.socket(zmq.PUB)
socket.connect("tcp://localhost:5559")

time.sleep(0.5) # Wait for sensor to initialize

while True:
  euler = map(str, sensor.euler)
  gravity = map(str, sensor.gravity)
  socket.send(f"IMU {' '.join(euler)} {' '.join(gravity)}".encode('UTF-8'))
