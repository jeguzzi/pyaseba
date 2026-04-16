import argparse
import asyncio
import sys

from pyaseba import ClientAsync


async def main(target: str, number: int) -> None:
    client = ClientAsync()
    if await client.connect(target, max_retries=10):
        print(f'Connected {target}')
        nodes = await client.wait_nodes(number=number, wait_ms=1000)
        print(f'Connected nodes {nodes}')
        tasks = [
            asyncio.Task(
                client.wait_node_disconnection(node_id=node_id,
                                               include={conn}))
            for conn, node_ids in nodes.items()
            for node_id in node_ids
        ]
        await asyncio.wait(tasks, timeout=1)
        client.close()
    else:
        raise RuntimeError(f"Could not connect to {target}!")


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:port=33333")
    parser.add_argument('--number', default=2, type=int)
    args = parser.parse_args()
    try:
        asyncio.run(main(args.target, args.number))
    except Exception as e:
        sys.exit(f"ERROR: {e}")

