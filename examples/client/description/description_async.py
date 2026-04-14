import argparse
import asyncio

from pyaseba import ClientAsync


async def main(target: str) -> None:
    client = ClientAsync()
    if await client.connect(target, max_retries=10):
        node_id, conn = await client.wait_node()
        if conn:
            description = client.get_description(node_id)
            assert description
            print(f"Node {node_id}: {description}")
            print(f"Variables: {description.variables}")
        client.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:host=127.0.0.1;port=33333")
    args = parser.parse_args()
    asyncio.run(main(args.target))
