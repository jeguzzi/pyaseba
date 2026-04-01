import argparse

from pyaseba import Client
import time


def main(target: str) -> None:
    client = Client()
    if client.connect(target, max_retries=1):
        while len(client.nodes) < 2:
            time.sleep(1)
        for node in client.nodes:
            if client.wait_node_connection(node=node, wait_ms=1000) is not None:
                description = client.get_description(node)
                print(f"Node {node}: {description}")
        client.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:host=127.0.0.1;port=33333")
    args = parser.parse_args()
    main(args.target)
