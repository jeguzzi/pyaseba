import argparse
import asyncio
import logging
import sys

from pyaseba import ClientAsync
from pyaseba.examples.utils import setup_logging


async def main(target: str, number: int) -> None:
    with ClientAsync(ping_period_ms=0, automatic_query=False) as client:
        if await client.connect(target, max_retries=1):
            nodes = await client.scan(wait_ms=1000, number=number)
            connections = client.connections
            for t, ns in nodes.items():
                logging.info(f'Found nodes {ns} on {connections.get(t, "?")}')
        else:
            raise RuntimeError(f"Could not connect to {target}!")


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:port=33333")
    parser.add_argument('--number', default=2, type=int)
    parser.add_argument('--log_level', default="INFO")
    args = parser.parse_args()
    setup_logging(args.log_level)
    try:
        asyncio.run(main(args.target, args.number))
    except Exception as e:
        logging.error(str(e))
        sys.exit(1)
