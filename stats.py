#!/usr/bin/python
import threading
import time
import zmq

counts = {}
last_counts = {}
running = True

def printer():
    while running:
        keys = list(counts.keys())
        keys.sort()
        for key in keys:
            if key in last_counts:
                diff = counts[key] - last_counts[key]
            else:
                diff = counts[key]
            last_counts[key] = counts[key]
            print(f"{key}: {diff} ", end="")
        print()
        time.sleep(1)

port = "5560"
# Socket to talk to server
context = zmq.Context()
socket = context.socket(zmq.SUB)
print("Collecting stats from server...")
socket.connect ("tcp://localhost:%s" % port)
topicfilter = ""
socket.setsockopt_string(zmq.SUBSCRIBE, topicfilter)

thread = threading.Thread(target=printer)
thread.isDaemon = True
thread.start()

try:
    while True:
        string = socket.recv()
        topic, message = string.decode('UTF-8').split(" ", 1)
        if not topic in counts:
            counts[topic] = 0
        counts[topic] += 1
except:
    running = False
