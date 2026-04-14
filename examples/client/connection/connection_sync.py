import argparse

from pyaseba import Client


def main(target: str) -> None:
    client = Client(max_protocol_version=9)
    if client.connect(target, max_retries=10):
        print(f'Connected {target}')
        node_id, connection = client.wait_node(wait_ms=1000)
        if connection:
            print(f'Connected node {node_id} on #{connection}')
            r, *_ = client.wait_node_disconnection(node_id=node_id,
                                                   wait_ms=1000,
                                                   include={connection})
            if r:
                print('Disconnected node')
        client.close()
    else:
        print(f"Could not connect to {target}")


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:host=127.0.0.1;port=33333")
    args = parser.parse_args()
    main(args.target)
