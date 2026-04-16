import argparse
import time
import sys

from pyaseba.client import Client, Message


def cb(node_ids: dict[int, set[int]], done: bool) -> None:
    print(f"Discovered {node_ids} {'...' if not done else '!'}")


def mcb(msg: Message, target: int) -> None:
    print(f"Got {msg}")


def main(target: str) -> None:
    client = Client()
    client.add_message_callback(mcb)
    if client.connect(target, max_retries=10):
        print(f'Connected {target}')
        client.wait_nodes(callback=cb, number=2, wait_ms=1000)
        time.sleep(0.2)
        client.close()
    else:
        raise RuntimeError(f"Could not connect to {target}!")


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:port=33333")
    args = parser.parse_args()
    try:
        main(args.target)
    except Exception as e:
        sys.exit(f"ERROR: {e}")
