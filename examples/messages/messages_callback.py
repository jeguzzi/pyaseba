import argparse
import time

from pyaseba import Network, Message


def cb(msg: Message) -> None:
    print(f"Received {msg}")


def main(target: str) -> None:
    network = Network()
    network.add_message_callback(cb)
    if network.connect(target, max_retries=10):
        time.sleep(2)
        network.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:host=127.0.0.1;port=33333")
    args = parser.parse_args()
    main(args.target)
