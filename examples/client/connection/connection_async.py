import argparse
import asyncio
import sys
import time

from pyaseba import ClientAsync


async def main(target: str, wait_ms: int) -> None:
    client = ClientAsync()
    if await client.connect(target, max_retries=0):
        print(f'Connected target {target}')
        a = time.time()
        node_id, connection = await client.wait_node(wait_ms=wait_ms)
        if not connection:
            raise RuntimeError("No node found!")
        print(f'Connected node {node_id} on #{connection} after {time.time() - a:.2f} s')
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
    parser.add_argument('--wait_ms', default=1000, type=int)
    args = parser.parse_args()
    try:
        asyncio.run(main(args.target, args.wait_ms))
    except Exception as e:
        sys.exit(f"ERROR: {e}")
