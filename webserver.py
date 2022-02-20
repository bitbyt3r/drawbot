#!/usr/bin/python
import sys
import zmq
import asyncio
import websockets
import zmq.asyncio
from aiostream import stream

context = zmq.asyncio.Context()
pub_socket = context.socket(zmq.PUB)
pub_socket.connect ("tcp://localhost:5559")

async def zmq_producer(socket):
    while True:
        available = await socket.poll()
        if available:
            yield await socket.recv_multipart()

async def ws_producer(ws):
    while True:
        msg = await ws.recv()
        yield msg

async def proxy(websocket, path):
    socket = context.socket(zmq.SUB)
    socket.connect(f"tcp://localhost:5560")
    socket.setsockopt_string(zmq.SUBSCRIBE, "LIDAR")
    while True:
        points = []
        available = await socket.poll()
        while available and len(points) < 36:
            packet = await socket.recv_multipart()
            parts = packet[0].decode('UTF-8').split(" ")
            points.append(f'{{"angle": {parts[1]}, "distance": {parts[2]}}}')
            available = await socket.poll()
        if points:
            await websocket.send(f'[{",".join(points)}]')

start_server = websockets.serve(proxy, '0.0.0.0', 8765)

asyncio.get_event_loop().run_until_complete(start_server)
asyncio.get_event_loop().run_forever()