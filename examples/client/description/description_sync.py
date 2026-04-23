import argparse
import logging
import sys

from pyaseba import Client, print_description
from pyaseba.examples.utils import setup_logging


def main(target: str) -> None:
    with Client() as client:
        if client.connect(target, max_retries=10):
            node_id, conn = client.wait_node(wait_ms=1000)
            if conn:
                description = client.get_description(node_id)
                if description:
                    print_description(node_id, description)
                else:
                    raise RuntimeError(f"No description for node {node_id}")
            else:
                raise RuntimeError("No node found!")
        else:
            raise RuntimeError(f"Could not connect to {target}!")


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:port=33333")
    parser.add_argument('--log_level', default="INFO")
    args = parser.parse_args()
    setup_logging(args.log_level)
    try:
        main(args.target)
    except Exception as e:
        logging.error(str(e))
        sys.exit(1)
