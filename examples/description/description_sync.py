import argparse

from pyaseba import Network


def main(target: str) -> None:
    network = Network()
    if network.connect(target, max_retries=10):
        node = network.wait_node_connection(wait_ms=1000)
        if node is not None:
            description = network.get_description(node)
            print(f"Node {node}: {description}")
        network.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:host=127.0.0.1;port=33333")
    args = parser.parse_args()
    main(args.target)
