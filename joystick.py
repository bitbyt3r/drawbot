#!/usr/bin/python3 -u
import evdev
import math
import time
import zmq
import os

DEVICE = "/dev/input/event0"
AXES = [(0, 1), (1, -1), (3, -1)]
RANGES = {0: [7200, 57584], 1: [8543, 61171], 3: [6768, 56608]}
DEADZONE = 0.1

print("Waiting for joystick...")
while not os.path.exists(DEVICE):
    time.sleep(1)
print("Joystick found!")

device = evdev.InputDevice(DEVICE)

context = zmq.Context()
socket = context.socket(zmq.PUB)
socket.connect ("tcp://localhost:5559")

last = time.time()

while True:
    vals = []
    for axis, direction in AXES:
        val = device.absinfo(axis).value
        if not axis in RANGES:
            RANGES[axis] = [val, val]
        if val > RANGES[axis][1]:
            RANGES[axis][1] = val
        if val < RANGES[axis][0]:
            RANGES[axis][0] = val
        width = (RANGES[axis][1] - RANGES[axis][0]) or 1
        interp = ((val - RANGES[axis][0]) / width) * 2 - 1
        interp *= direction
        vals.append(interp)
    vals = [x if abs(x) > DEADZONE else 0 for x in vals]
    x, y, theta = vals
    theta *= 2048
    angle = math.atan2(y, x)
    magnitude = math.sqrt(y**2 + x**2) * 2048
    fr = int(math.sin(angle + math.pi / 4)*magnitude + theta)
    rl = int(math.sin(angle + math.pi / 4)*magnitude - theta)
    fl = int(math.sin(angle - math.pi / 4)*magnitude - theta)
    rr = int(math.sin(angle - math.pi / 4)*magnitude + theta)
    if any([fr, rl, fl, rr]):
        last = time.time()
    if time.time() - last < 3:
        socket.send(f"DRIVE {fr} {fl} {rl} {rr} 0".encode("UTF-8"))
    time.sleep(0.01)