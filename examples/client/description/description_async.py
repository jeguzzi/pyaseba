import argparse
import asyncio
import sys

from pyaseba import ClientAsync, print_description


async def main(target: str) -> None:
    client = ClientAsync()
    if await client.connect(target, max_retries=10):
        node_id, conn = await client.wait_node(wait_ms=1000)
        if conn:
            description = client.get_description(node_id)
            if description:
                print_description(node_id, description)
            else:
                raise RuntimeError(f"No description for node {node_id}")
        else:
            raise RuntimeError("No node found!")
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
