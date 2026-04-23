import argparse
import asyncio
import logging
import sys

from pyaseba import ClientAsync
from pyaseba.examples.utils import setup_logging


async def main(target: str, number: int, wait_ms: int) -> None:
    with ClientAsync() as client:
        if await client.connect(target, max_retries=1):
            logging.info(f'Connected {target}')
            nodes = await client.wait_nodes(number=number, wait_ms=wait_ms)
            logging.info(f'Connected nodes {nodes}')
            tasks = [
                asyncio.Task(
                    client.wait_node_disconnection(node_id=node_id,
                                                   include={conn}))
                for conn, node_ids in nodes.items() for node_id in node_ids
            ]
            await asyncio.wait(tasks, timeout=1)
        else:
            raise RuntimeError(f"Could not connect to {target}!")


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:port=33333")
    parser.add_argument('--number', default=2, type=int)
    parser.add_argument('--wait', default=1000, type=int)
    parser.add_argument('--log_level', default="INFO")
    args = parser.parse_args()
    setup_logging(args.log_level)
    try:
        asyncio.run(main(args.target, args.number, args.wait))
    except Exception as e:
        logging.error(str(e))
        sys.exit(1)
