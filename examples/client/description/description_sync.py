import argparse

from pyaseba import Client
from pyaseba.client import print_description


def main(target: str) -> None:
    client = Client()
    if client.connect(target, max_retries=10):
        node_id, conn = client.wait_node(wait_ms=1000)
        if conn:
            description = client.get_description(node_id)
            assert description
            print_description(node_id, description)
        client.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:host=127.0.0.1;port=33333")
    args = parser.parse_args()
    main(args.target)
