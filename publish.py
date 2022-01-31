#!/usr/bin/python
import readline
import atexit
import sys
import zmq
import os

histfile = os.path.join(os.path.expanduser("~"), ".publish_history")
try:
    readline.read_history_file(histfile)
except:
    pass
atexit.register(readline.write_history_file, histfile)

port = "5559"
# Socket to talk to server
context = zmq.Context()
socket = context.socket(zmq.PUB)
socket.connect ("tcp://localhost:%s" % port)
while True:
    string = input(">")
    socket.send(string.encode("UTF-8"))
