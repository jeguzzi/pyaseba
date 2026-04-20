import argparse
import time
import sys

from pyaseba.client import Client, Message


def cb(node_ids: dict[int, set[int]], done: bool) -> None:
    print(f"Discovered {node_ids} {'...' if not done else '!'}")


def mcb(msg: Message, target: int) -> None:
    print(f"Got {msg}")


def main(target: str, wait_ms: int) -> None:
    client = Client()
    client.add_message_callback(mcb)
    if client.connect(target, max_retries=1):
        print(f'Connected {target}')
        client.wait_nodes(callback=cb, number=2, wait_ms=wait_ms)
        time.sleep(0.2)
        client.close()
    else:
        raise RuntimeError(f"Could not connect to {target}!")


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:port=33333")
    parser.add_argument('--wait_ms', default=1000, type=int)
    args = parser.parse_args()
    try:
        main(args.target, args.wait_ms)
    except Exception as e:
        sys.exit(f"ERROR: {e}")
