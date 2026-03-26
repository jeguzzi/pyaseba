import argparse
import asyncio

from pyaseba import NetworkAsync


async def main(target: str) -> None:
    network = NetworkAsync()
    if await network.connect(target, max_retries=10):
        print(f'Connected {target}')
        node = await network.wait_node_connection()
        print(f'Connected node {node}')
        task = asyncio.Task(network.wait_disconnection())
        await asyncio.wait([task], timeout=5)
        network.close()
    else:
        print(f"Could not connect to {target}")


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:host=127.0.0.1;port=33333")
    args = parser.parse_args()
    asyncio.run(main(args.target))
