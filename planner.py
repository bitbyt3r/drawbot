#!/usr/bin/python
import readline
import atexit
import numpy as np
import time
import math
import sys
import zmq
import os 

histfile = os.path.join(os.path.expanduser("~"), ".publish_history")
try:
    readline.read_history_file(histfile)
except:
    pass
atexit.register(readline.write_history_file, histfile)

OMEGA = 0.01

context = zmq.Context()
socket = context.socket(zmq.PUB)
socket.connect ("tcp://localhost:5559")
print("Enter waypoints as X Y Theta, then enter a blank line to execute.")
points = []
while True:
    string = input(">")
    if not string:
        break
    try:
        x, y, theta = map(float, string.split(" "))
        points.append(np.array([x, y, theta]))
    except:
        print(f"Could not parse '{string}'.")

def max_velocity(heading):
    return 0.1
    # finds the max relative velocity when traveling in the given direction

def max_acceleration(heading):
    return 0.1
    # finds the max relative acceleration when traveling in the given direction

trajectory = []
print(points)
original_start = time.time()
for start, end in zip(points, points[1:]):
    print(f"Planning from {start} to {end}")
    start_time = time.time()
    path = end - start
    distance = np.linalg.norm(path)
    heading = path / distance # normalized path vector
    max_v = max_velocity(heading)
    max_a = max_acceleration(heading)

    accel_time = max_v / max_a
    accel_distance = 0.5 * max_a * accel_time**2
    if distance > 2*accel_distance: # If true then there is a coast phase
        travel_time = accel_time * 2 + (distance - 2*accel_distance) / max_v
    else: # There is no coast phase, just ramp up and back down
        travel_time = math.sqrt(distance / max_a)*2
        accel_time = travel_time / 2
        accel_distance = 0.5 * max_a * accel_time**2

    while (time.time() - start_time) < accel_time:
        travelled_distance = 0.5 * max_a * (time.time() - start_time)**2
        goal = start + heading * travelled_distance
        #trajectory.append([time.time() - original_start, goal[0], goal[1], goal[2]])
        socket.send(f"GOAL {goal[0]} {goal[1]} {goal[2]}".encode('UTF-8'))
        time.sleep(OMEGA)
    
    while (travel_time - accel_time) > (time.time() - start_time):
        goal = start + heading * (accel_distance + max_v * ((time.time() - start_time) - accel_time))
        #trajectory.append([time.time() - original_start, goal[0], goal[1], goal[2]])
        socket.send(f"GOAL {goal[0]} {goal[1]} {goal[2]}".encode('UTF-8'))
        time.sleep(OMEGA)

    while travel_time > (time.time() - start_time):
        remaining_distance = 0.5 * max_a * (travel_time - (time.time() - start_time))**2
        goal = end - heading * remaining_distance
        #trajectory.append([time.time() - original_start, goal[0], goal[1], goal[2]])
        socket.send(f"GOAL {goal[0]} {goal[1]} {goal[2]}".encode('UTF-8'))
        time.sleep(OMEGA)

# with open("trajectory.csv", "w") as FILE:
#     FILE.write("Time,X,Y,Theta\n")
#     for point in trajectory:
#         strs = map(str, point)
#         FILE.write(f"{','.join(strs)}\n")
