import argparse
import time

from pyaseba import Client
from pyaseba.client import Message


def cb(msg: Message, target: int) -> None:
    print(f"Received {msg}")


def main(target: str) -> None:
    client = Client()
    client.add_message_callback(cb)
    if client.connect(target, max_retries=10):
        time.sleep(5)
        client.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:host=127.0.0.1;port=33333")
    args = parser.parse_args()
    main(args.target)
