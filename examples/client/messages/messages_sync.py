import argparse
import logging
import sys

from pyaseba import Client
from pyaseba.examples.utils import setup_logging


def main(target: str) -> None:
    with Client() as client:
        if client.connect(target, max_retries=10):
            for _ in range(5):
                msg, conn = client.get_message()
                logging.info(f"Received {msg} from #{conn}")
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
