#!/usr/bin/python
import readline
import time
import sys
import zmq

port = "5560"
# Socket to talk to server
context = zmq.Context()
socket = context.socket(zmq.SUB)
print("Collecting updates from server...")
socket.connect ("tcp://localhost:%s" % port)
if len(sys.argv) == 1:
  topicfilter = ""
  socket.setsockopt_string(zmq.SUBSCRIBE, topicfilter)
else:
    for arg in sys.argv[1:]:
        socket.setsockopt_string(zmq.SUBSCRIBE, arg)
while True:
    string = socket.recv().decode('UTF-8')
    print(time.time(), string)
