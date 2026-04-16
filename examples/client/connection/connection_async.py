import argparse
import asyncio
import sys

from pyaseba import ClientAsync


async def main(target: str) -> None:
    client = ClientAsync()
    if await client.connect(target, max_retries=1):
        print(f'Connected target {target}')
        node_id, connection = await client.wait_node(wait_ms=1000)
        if not connection:
            raise RuntimeError("No node found!")
        print(f'Connected node {node_id} on #{connection}')
        node_id, connection = await client.wait_node_disconnection(
            node_id=node_id, wait_ms=1000, include={connection})
        if connection:
            print(f'Disconnected node {node_id}')
        client.close()
    else:
        raise RuntimeError(f"Could not connect to {target}!")


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:port=33333")
    args = parser.parse_args()
    try:
        asyncio.run(main(args.target))
    except Exception as e:
        sys.exit(f"ERROR: {e}")
