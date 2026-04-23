import argparse
import logging
import sys

from pyaseba import Client
from pyaseba.examples.utils import setup_logging


def main(target: str) -> None:
    with Client(port=33334) as client:
        if target:
            client.connect(target, max_retries=0)
        logging.info(f"Connections: {client.connections}")
        connection, new_target = client.wait_connection(wait_ms=1000)
        if connection:
            logging.info(f'Added connection #{connection} to {new_target}')
            r = client.wait_disconnection(connection, wait_ms=1000)
            if r:
                logging.info("Disconnected")
            else:
                logging.info("Not yet disconnected")
        else:
            logging.info("No new connections")


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
