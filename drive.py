#!/usr/bin/python
import numpy as np
import math
import time
import sys
import zmq

WHEEL_DISTANCE = sum([0.049, 0.1175])
ENCODER_PPR = 450
WHEEL_RADIUS = 0.040

context = zmq.Context()
sub_socket = context.socket(zmq.SUB)
sub_socket.connect ("tcp://localhost:5560")
sub_socket.setsockopt_string(zmq.SUBSCRIBE, "WHEEL_ENCODER")
sub_socket.setsockopt_string(zmq.SUBSCRIBE, "GOAL")

pub_socket = context.socket(zmq.PUB)
pub_socket.connect ("tcp://localhost:5559")

def inverse_kinematics(move):
    x, y, theta = move
    fr = (y - x + WHEEL_DISTANCE*theta) / WHEEL_RADIUS
    fl = (y + x - WHEEL_DISTANCE*theta) / WHEEL_RADIUS
    rl = (y - x - WHEEL_DISTANCE*theta) / WHEEL_RADIUS
    rr = (y + x + WHEEL_DISTANCE*theta) / WHEEL_RADIUS
    return np.array([fr, fl, rl, rr])

def forward_kinematics(wheel_rotations):
    fr, fl, rl, rr = wheel_rotations
    y = sum([fr, fl, rl, rr]) * WHEEL_RADIUS / 4
    x = ((fl + rr) - (fr + rl)) * WHEEL_RADIUS / 4
    theta = ((fr + rr) - (fl + rl)) * WHEEL_RADIUS / (4 * WHEEL_DISTANCE)
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
wheel_vel = np.array([0, 0, 0, 0])
last_pulse_time = [goal_time, goal_time, goal_time, goal_time]
time.sleep(1) # Let some time pass to minimize startup impulse
i = 0

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
    elif command.startswith("WHEEL_ENCODER"):
        topic, fr, rl, fl, rr = command.split(" ")
        new_encoders = np.array([int(fr), int(rl), int(fl), int(rr)])
        encoder_diff = new_encoders - encoders
        encoders = new_encoders
        now = time.time()
        for idx, encoder in enumerate(encoder_diff):
            elapsed_time = now - last_pulse_time[idx]
            if encoder != 0:
                last_pulse_time[idx] = now
                wheel_vel[idx] = ((2*math.pi * encoder)/ENCODER_PPR)/elapsed_time
            else:
                wheel_vel[idx] = 0
        relative_pos = forward_kinematics(wheel_vel*(now - pos_time))
        pos_time = now
        vel = (relative_pos / elapsed_time)*0.1 + vel*0.9
        theta = pos[2] + relative_pos[2]
        x = relative_pos[0] * math.cos(theta) + pos[0] + relative_pos[1] * math.sin(theta)
        y = relative_pos[0] * math.sin(theta) + pos[1] + relative_pos[1] * math.cos(theta)
        pos = [x, y, theta]
    error = goal - pos
    error_vel = goal_vel - vel
    speeds = np.clip(inverse_kinematics(error)*512, -1024, 1024)
    i += 1
    if i%100==0:
        print(f"DRIVE {int(speeds[0])} {int(speeds[1])} {int(speeds[2])} {int(speeds[3])} 0", pos)
        pub_socket.send(f"DRIVE {int(speeds[0])} {int(speeds[1])} {int(speeds[2])} {int(speeds[3])} 0".encode("UTF-8"))
    #pub_socket.send(f"DRIVE {fr} {fl} {rl} {rr} 0".encode("UTF-8"))

