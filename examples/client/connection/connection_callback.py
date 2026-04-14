import argparse
import time
from functools import partial

from pyaseba import Client


def cb(node_id: int, target: int, title: str = "") -> None:
    print(f"{title} {node_id} {target}")


def main(target: str) -> None:
    client = Client()
    if client.connect(target, max_retries=10):
        print(f'Connected {target}')
        client.add_node_callback(partial(cb, title="Connected"))
        client.add_node_disconnection_callback(partial(cb, title="Disconnected"))
        time.sleep(10)
        client.close()
    else:
        print(f"Could not connect to {target}")


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:host=127.0.0.1;port=33333")
    args = parser.parse_args()
    main(args.target)
