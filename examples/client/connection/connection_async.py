import argparse
import asyncio
import logging
import sys

from pyaseba import ClientAsync
from pyaseba.examples.utils import setup_logging


async def main(target: str, wait_ms: int) -> None:
    with ClientAsync() as client:
        if await client.connect(target, max_retries=0):
            logging.info(f'Connected target {target}')
            node_id, connection = await client.wait_node(wait_ms=wait_ms)
            if not connection:
                raise RuntimeError("No node found!")
            logging.info(f'Connected node {node_id} on #{connection}')
            node_id, connection = await client.wait_node_disconnection(
                node_id=node_id, wait_ms=1000, include={connection})
            if connection:
                logging.info(f'Disconnected node {node_id}')
        else:
            raise RuntimeError(f"Could not connect to {target}!")


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:port=33333")
    parser.add_argument('--wait_ms', default=1000, type=int)
    parser.add_argument('--log_level', default="INFO")
    args = parser.parse_args()
    setup_logging(args.log_level)
    try:
        asyncio.run(main(args.target, args.wait_ms))
    except Exception as e:
        logging.error(str(e))
        sys.exit(1)
