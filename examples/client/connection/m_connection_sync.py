import argparse
import logging
import sys

from pyaseba import Client
from pyaseba.examples.utils import setup_logging


def cb(nodes: dict[int, set[int]], complete: bool) -> None:
    logging.info(f'cb: {nodes} ({complete})')


def main(target: str, number: int, wait_ms: int) -> None:
    with Client() as client:
        if client.connect(target, max_retries=1):
            logging.info(f'Connected {target}')
            nodes = client.wait_nodes(number=number, wait_ms=wait_ms)
            logging.info(f'Connected nodes {nodes}')
            for conn, node_ids in nodes.items():
                for node_id in node_ids:
                    client.wait_node_disconnection(wait_ms=1000,
                                                   node_id=node_id,
                                                   include={conn})
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
        main(args.target, args.number, args.wait)
    except Exception as e:
        logging.error(str(e))
        sys.exit(1)
