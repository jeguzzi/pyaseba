import argparse
import sys
import time
from functools import partial

from pyaseba import Client


def cb(node_id: int, connection: int, title: str = "") -> None:
    print(f"{title} {node_id} on #{connection}")


def main(target: str) -> None:
    client = Client()
    if client.connect(target, max_retries=10):
        print(f'Connected {target}')
        client.add_node_callback(partial(cb, title="Connected"))
        client.add_node_disconnection_callback(partial(cb, title="Disconnected"))
        time.sleep(2)
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

