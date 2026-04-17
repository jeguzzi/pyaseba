import argparse
import sys

from pyaseba import Client


def cb(nodes: dict[int, set[int]], complete: bool) -> None:
    print(f'cb: {nodes} ({complete})')


def main(target: str, number: int, wait_ms: int) -> None:
    client = Client()
    if client.connect(target, max_retries=1):
        print(f'Connected {target}')
        nodes = client.wait_nodes(number=number, wait_ms=wait_ms)
        print(f'Connected nodes {nodes}')
        for conn, node_ids in nodes.items():
            for node_id in node_ids:
                client.wait_node_disconnection(wait_ms=1000, node_id=node_id, include={conn})
        client.close()
    else:
        raise RuntimeError(f"Could not connect to {target}!")


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:port=33333")
    parser.add_argument('--number', default=2, type=int)
    parser.add_argument('--wait', default=1000, type=int)
    args = parser.parse_args()
    try:
        main(args.target, args.number, args.wait)
    except Exception as e:
        sys.exit(f"ERROR: {e}")
