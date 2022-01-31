#!/usr/bin/python3
import evdev
import time
import zmq

DEVICE = "/dev/input/event0"
AXES = [(0, 1), (1, -1), (3, -1)]

ranges = {}
device = evdev.InputDevice(DEVICE)

context = zmq.Context()
socket = context.socket(zmq.PUB)
socket.connect ("tcp://localhost:5559")

while True:
    vals = []
    for axis, direction in AXES:
        val = device.absinfo(axis).value
        if not axis in ranges:
            ranges[axis] = [val, val]
        if val > ranges[axis][1]:
            ranges[axis][1] = val
        if val < ranges[axis][0]:
            ranges[axis][0] = val
        width = (ranges[axis][1] - ranges[axis][0]) or 1
        interp = ((val - ranges[axis][0]) / width) * 2 - 1
        interp *= direction
        vals.append(str(interp))
    socket.send(f"JOYSTICK {' '.join(vals)}".encode('UTF-8'))
    time.sleep(0.01)