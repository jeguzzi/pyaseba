import argparse
import logging
import sys
import time
from functools import partial

from pyaseba import Client
from pyaseba.examples.utils import setup_logging


def cb(conn: int, name: str, title: str = "") -> None:
    logging.info(f"{title} connection #{conn} to {name}")


def main(target: str) -> None:
    with Client(port=33334) as client:
        if target:
            client.connect(target, max_retries=0)
        logging.info(f"Connections: {client.connections}")
        client.add_connection_callback(partial(cb, title="Added"))
        client.add_disconnection_callback(partial(cb, title="Removed"))
        time.sleep(1)


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
