import asyncio
import sys

from pyaseba import ClientAsync


async def run(client: ClientAsync) -> None:
    count = 0
    while count < 10:
        msg, target = await client.get_message()
        if msg:
            if msg.type in (0, 1):
                print(f'Client on port {client.port} received {msg}')
                await asyncio.sleep(0.1)
                client.send_user_message(type=client.port % 10,
                                         payload=[count])
                count += 1


async def main() -> None:
    ports = [10000, 10001]
    tasks: list[asyncio.Task[None]] = []
    clients: list[ClientAsync] = []
    for port in ports:
        client = ClientAsync(port=port, address="localhost")
        task = asyncio.create_task(run(client))
        tasks.append(task)
        clients.append(client)
    for client, port in zip(clients, ports[::-1]):
        r = await client.connect(f"tcp:port={port};host=localhost", ping=False)
        assert r
    clients[0].send_user_message(type=clients[0].port % 10)
    try:
        done, pending = await asyncio.wait(tasks, timeout=3)
        if pending:
            print(f'ERROR: {len(pending)} still pending')
    except Exception:
        pass
    finally:
        for client in clients:
            client.close()


if __name__ == '__main__':
    try:
        asyncio.run(main())
    except Exception as e:
        sys.exit(f"ERROR: {e}")
