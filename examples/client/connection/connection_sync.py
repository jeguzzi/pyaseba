import argparse
import sys

from pyaseba import Client


def main(target: str) -> None:
    client = Client(max_protocol_version=9)
    if client.connect(target, max_retries=10):
        print(f'Connected {target}')
        node_id, connection = client.wait_node(wait_ms=1000)
        if not connection:
            raise RuntimeError("No node found!")
        print(f'Connected node {node_id} on #{connection}')
        node_id, connection = client.wait_node_disconnection(
            node_id=node_id, wait_ms=1000, include={connection})
        if connection:
            print(f'Disconnected node {node_id}')
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
