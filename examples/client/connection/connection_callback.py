import argparse
import logging
import sys
import time
from functools import partial

from pyaseba import Client
from pyaseba.examples.utils import setup_logging


def cb(node_id: int, connection: int, title: str = "") -> None:
    logging.info(f"{title} {node_id} on #{connection}")


def main(target: str, wait_ms: int) -> None:
    with Client() as client:
        if client.connect(target, max_retries=10):
            logging.info(f'Connected {target}')
            client.add_node_callback(partial(cb, title="Connected"))
            client.add_node_disconnection_callback(
                partial(cb, title="Disconnected"))
            time.sleep(wait_ms * 1e-3)
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
        main(args.target, args.wait_ms)
    except Exception as e:
        logging.error(str(e))
        sys.exit(1)
