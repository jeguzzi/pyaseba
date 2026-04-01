import argparse

from pyaseba import Client


def main(target: str) -> None:
    client = Client()
    if client.connect(target, max_retries=10):
        print(f'Connected {target}')
        node = client.wait_node_connection(wait_ms=1000)
        print(f'Connected node {node}')
        client.wait_disconnection(wait_ms=1000)
        client.close()
    else:
        print(f"Could not connect to {target}")


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:host=127.0.0.1;port=33333")
    args = parser.parse_args()
    main(args.target)
