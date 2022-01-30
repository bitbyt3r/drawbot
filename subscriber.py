#!/usr/bin/python

import sys
import zmq

port = "5560"
# Socket to talk to server
context = zmq.Context()
socket = context.socket(zmq.SUB)
print("Collecting updates from server...")
socket.connect ("tcp://localhost:%s" % port)
topicfilter = ""
socket.setsockopt_string(zmq.SUBSCRIBE, topicfilter)
while True:
    string = socket.recv()
    print(string)
