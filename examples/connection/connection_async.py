import argparse
import asyncio

from pyaseba import ClientAsync


async def main(target: str) -> None:
    client = ClientAsync()
    if await client.connect(target, max_retries=10):
        print(f'Connected {target}')
        node = await client.wait_node_connection()
        print(f'Connected node {node}')
        task = asyncio.Task(client.wait_disconnection())
        await asyncio.wait([task], timeout=5)
        client.close()
    else:
        print(f"Could not connect to {target}")


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:host=127.0.0.1;port=33333")
    args = parser.parse_args()
    asyncio.run(main(args.target))
