#!./venv/bin/python

"""
execute the following line in a venv to install the necesary dependency
pip install websockets
"""

import asyncio

from websockets.asyncio.server import serve

async def hello(websocket):
    print("Hello from python websocket server!")
    await websocket.send("Hello from python")
    name = await websocket.recv()
    print(f"<<< {name}")

    greeting = f"Hello {name}!"

    await websocket.send(greeting)
    print(f">>> {greeting}")

async def main():
    async with serve(hello, "localhost", 8765) as server:
        await server.serve_forever()

if __name__ == "__main__":
    asyncio.run(main())
