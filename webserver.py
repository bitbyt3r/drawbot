#!/usr/bin/python
import sys
import zmq
import json
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
    zmq_prod = zmq_producer(socket)
    ws_prod = ws_producer(websocket)

    events = stream.merge(zmq_prod, ws_prod)
    async with events.stream() as streamer:
        async for item in streamer:
            if type(item) is list:
                string = item[0].decode('UTF-8')
                parts = string.split(" ")
                if parts[0] == "LIDAR":
                    data = {
                        "angle": float(parts[1]),
                        "distance": float(parts[2]),
                        "quality": int(parts[3])
                    }
                    await websocket.send(json.dumps(data))
            else:
                await pub_socket.send(item.encode('UTF-8'))

start_server = websockets.serve(proxy, '0.0.0.0', 8765)

asyncio.get_event_loop().run_until_complete(start_server)
asyncio.get_event_loop().run_forever()