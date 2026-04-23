import argparse
import logging
import sys

from pyaseba.client import Client
from pyaseba.examples.utils import setup_logging


def main(target: str, number: int) -> None:
    with Client(ping_period_ms=0, automatic_query=False) as client:
        if client.connect(target, max_retries=1):
            nodes = client.scan(wait_ms=1000, number=number)
            for conn, node_ids in nodes.items():
                for node_id in node_ids:
                    desc = client.query_description(node_id=node_id,
                                                    wait_ms=10000,
                                                    include={conn})
                    if desc:
                        logging.info(
                            f"Connected node with id {node_id} and name {desc.name}"
                        )
                    else:
                        raise RuntimeError("Could not query node {node_id}")
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
        main(args.target, args.number)
    except Exception as e:
        logging.error(str(e))
        sys.exit(1)
