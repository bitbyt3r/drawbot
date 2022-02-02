#!/usr/bin/python
import numpy as np
import math
import time
import sys
import zmq

WHEEL_POSITION = [49, 117.5]
ENCODER_PPR = 512
WHEEL_RADIUS = 40

context = zmq.Context()
sub_socket = context.socket(zmq.SUB)
sub_socket.connect ("tcp://localhost:5560")
topicfilter = "WHEEL_ENCODER"
sub_socket.setsockopt_string(zmq.SUBSCRIBE, topicfilter)

pub_socket = context.socket(zmq.PUB)
pub_socket.connect ("tcp://localhost:5559")

def inverse_kinematics(x, y, theta):
    angle = math.atan2(y, x)
    magnitude = math.sqrt(y**2 + x**2)
    fr = int(math.sin(angle + math.pi / 4)*magnitude + theta)
    rl = int(math.sin(angle + math.pi / 4)*magnitude - theta)
    fl = int(math.sin(angle - math.pi / 4)*magnitude - theta)
    rr = int(math.sin(angle - math.pi / 4)*magnitude + theta)
    return fr, fl, rl, rr

def forward_kinematics(wheel_rotations):
    fr, fl, rl, rr = wheel_rotations
    x = sum([fr, fl, rl, rr]) * WHEEL_RADIUS / 4
    y = ((fr + rl) - (fl + rr)) * WHEEL_RADIUS / 4
    theta = ((fr + rr) - (fl + rl)) * WHEEL_RADIUS / (4 * (sum(WHEEL_POSITION)))
    return np.array([x, y, theta])

goal = np.array([0, 0, 0])
goal_time = time.time()
goal_vel = np.array([0, 0, 0])
pos = np.array([0, 0, 0])
pos_time = time.time()
vel = np.array([0, 0, 0])
error = np.array([0, 0, 0])
error_vel = np.array([0, 0, 0])
encoders = np.array([0, 0, 0, 0])

while True:
    command = sub_socket.recv().decode('UTF-8')
    if command.startswith("GOAL"):
        topic, x, y, theta = command.split(" ")
        new_goal = np.array([float(x), float(y), float(theta)])
        now = time.time()
        elapsed_time = now - goal_time
        goal_time = now
        goal_vel = (new_goal - goal) / elapsed_time
        goal = new_goal
        error = goal - pos
        error_vel = goal_vel - vel
    elif command.startswith("WHEEL_ENCODER"):
        topic, fr, rl, fl, rr = command.split(" ")
        new_encoders = np.array([int(fr), int(rl), int(fl), int(rr)])
        encoder_diff = new_encoders - encoders
        print(encoder_diff)
        encoders = new_encoders
        now = time.time()
        elapsed_time = now - pos_time
        pos_time = now
        wheel_rotations = [((2*math.pi * x)/ENCODER_PPR)/elapsed_time for x in encoders]
        new_pos = forward_kinematics(wheel_rotations) + pos
        vel = (new_pos - pos) / elapsed_time
        pos = new_pos
        error = goal - pos
        error_vel = goal_vel - vel
    #print(pos)
    #pub_socket.send(f"DRIVE {fr} {fl} {rl} {rr} 0".encode("UTF-8"))

