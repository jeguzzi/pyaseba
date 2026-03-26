import argparse
import time
from functools import partial

from pyaseba import Network


def cb(node_id: int, title: str = "") -> None:
    print(f"{title} {node_id}")


def main(target: str) -> None:
    network = Network()
    if network.connect(target, max_retries=10):
        print(f'Connected {target}')
        network.add_connection_callback(partial(cb, title="Connected"))
        network.add_disconnection_callback(partial(cb, title="Disconnected"))
        time.sleep(1)
        network.close()
    else:
        print(f"Could not connect to {target}")


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:host=127.0.0.1;port=33333")
    args = parser.parse_args()
    main(args.target)
