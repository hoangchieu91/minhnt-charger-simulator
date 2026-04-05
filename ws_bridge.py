import asyncio
import websockets

async def renode_to_ws(websocket):
    try:
        # Connect strictly to Renode's ServerSocketTerminal at port 4321
        reader, writer = await asyncio.open_connection('127.0.0.1', 4321)
        print("Connected to Renode Simulator UART3!")
        while True:
            data = await reader.readline()
            if not data: break
            # Parse the incoming string and push directly to Dashboard Websocket
            await websocket.send(data.decode('utf-8', 'ignore').strip())
    except Exception as e:
        print("Renode Connection Loss:", e)
        await asyncio.sleep(2)

async def main():
    print("Starting Websocket Bridge on ws://localhost:4322 ...")
    async with websockets.serve(renode_to_ws, "0.0.0.0", 4322):
        await asyncio.Future()  # run forever

asyncio.run(main())
