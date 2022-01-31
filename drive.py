#!/usr/bin/python
import math
import sys
import zmq

context = zmq.Context()
sub_socket = context.socket(zmq.SUB)
sub_socket.connect ("tcp://localhost:5560")
topicfilter = "JOYSTICK"
sub_socket.setsockopt_string(zmq.SUBSCRIBE, topicfilter)

pub_socket = context.socket(zmq.PUB)
pub_socket.connect ("tcp://localhost:5559")

while True:
    command = sub_socket.recv().decode('UTF-8')
    if command.startswith("JOYSTICK"):
        topic, x, y, theta = command.split(" ")
        x = float(x)
        y = float(y)
        theta = float(theta) * 2048
        angle = math.atan2(y, x)
        magnitude = math.sqrt(y**2 + x**2) * 2048
        fr = int(math.sin(angle + math.pi / 4)*magnitude + theta)
        rl = int(math.sin(angle + math.pi / 4)*magnitude - theta)
        fl = int(math.sin(angle - math.pi / 4)*magnitude - theta)
        rr = int(math.sin(angle - math.pi / 4)*magnitude + theta)
    pub_socket.send(f"DRIVE {fr} {fl} {rl} {rr} 0".encode("UTF-8"))
    # string = socket.recv().decode('UTF-8')

