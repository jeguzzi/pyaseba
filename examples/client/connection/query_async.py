import argparse
import asyncio
import sys

from pyaseba.client import ClientAsync


async def main(target: str, number: int) -> None:
    client = ClientAsync(ping_period_ms=0, automatic_query=False)
    print(client.ping_period_ms, client.automatic_query)
    if await client.connect(target, max_retries=1):
        nodes = await client.scan(wait_ms=1000, number=number)
        print(f"Found nodes {nodes}")
        for conn, node_ids in nodes.items():
            for node_id in node_ids:
                desc = await client.query_description(node_id=node_id, include={conn}, wait_ms=1000)
                if desc:
                    print(f"Connected node with id {node_id} and name {desc.name}")
                else:
                    raise RuntimeError("Could not query node {node_id}")
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
