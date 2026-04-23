import argparse
import logging
import sys
import time

from pyaseba.client import Client, Message
from pyaseba.examples.utils import setup_logging


def cb(node_ids: dict[int, set[int]], done: bool) -> None:
    logging.info(f"Discovered {node_ids} {'...' if not done else '!'}")


def mcb(msg: Message, target: int) -> None:
    logging.info(f"Got {msg}")


def main(target: str, wait_ms: int) -> None:
    with Client() as client:
        client.add_message_callback(mcb)
        if client.connect(target, max_retries=1):
            logging.info(f'Connected {target}')
            client.wait_nodes(callback=cb, number=2, wait_ms=wait_ms)
            time.sleep(0.2)
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
