import argparse
import sys
import time
from functools import partial

from pyaseba import Client


def cb(conn: int, name: str, title: str = "") -> None:
    print(f"{title} connection #{conn} to {name}")


def main(target: str) -> None:
    client = Client(port=33334)
    if target:
        client.connect(target, max_retries=0)
    print(f"Connections: {client.connections}")
    client.add_connection_callback(partial(cb, title="Added"))
    client.add_disconnection_callback(partial(cb, title="Removed"))
    time.sleep(1)
    client.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:port=33333")
    args = parser.parse_args()
    try:
        main(args.target)
    except Exception as e:
        sys.exit(f"ERROR: {e}")
