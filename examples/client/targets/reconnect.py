import argparse
import logging
import sys

from pyaseba import Client
from pyaseba.examples.utils import setup_logging


def main(target: str) -> None:
    with Client(port=33334) as client:
        for _ in range(3):
            logging.info("Connecting")
            connection = client.connect(target, max_retries=1)
            logging.info(f"Connections: {client.connections}")
            if connection:
                logging.info(f"Closing connection #{connection}")
                r = client.close_connection(connection)
                if not r:
                    raise RuntimeError("Failed closing connection")
                logging.info(f"Connections: {client.connections}")
            else:
                raise RuntimeError("Could not connect")


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
