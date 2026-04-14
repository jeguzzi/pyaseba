import argparse

from pyaseba import Client


def main(target: str) -> None:
    client = Client()
    if client.connect(target, max_retries=1):
        client.wait_nodes(number=2, wait_ms=50000)
        for conn, node_ids in client.node_ids.items():
            for node_id in node_ids:
                description = client.get_description(node_id)
                print(f"Node {node_id}: {description}")
        client.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:host=127.0.0.1;port=33333")
    args = parser.parse_args()
    main(args.target)
