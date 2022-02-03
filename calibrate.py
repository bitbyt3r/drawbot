#!/usr/bin/python
import numpy as np
import math
import time
import sys
import zmq

WHEEL_POSITION = [0.049, 0.1175]
ENCODER_PPR = 512
WHEEL_RADIUS = 0.040

context = zmq.Context()
sub_socket = context.socket(zmq.SUB)
sub_socket.connect ("tcp://localhost:5560")
topicfilter = "WHEEL_ENCODER"
sub_socket.setsockopt_string(zmq.SUBSCRIBE, topicfilter)

start = [0, 0, 0, 0]
command = sub_socket.recv().decode('UTF-8')
topic, fr, rl, fl, rr = command.split(" ")
start = np.array([int(fr), int(rl), int(fl), int(rr)])
i = 0

while True:
    i += 1
    command = sub_socket.recv().decode('UTF-8')
    topic, fr, rl, fl, rr = command.split(" ")
    new_encoders = np.array([int(fr), int(rl), int(fl), int(rr)])
    encoder_diff = new_encoders - start
    if i % 100 == 0:
        print(encoder_diff)
