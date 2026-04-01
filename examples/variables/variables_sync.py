import argparse

from pyaseba import Client


def main(target: str) -> None:
    client = Client()
    if client.connect(target, max_retries=10):
        node = client.wait_node_connection(wait_ms=5000)
        if node is not None:
            description = client.get_description(node)
            if description:
                name, size = description.variables[0]
                value = client.get_variable(node, name, wait_ms=1000)
                print(f"Variable {name} = {value}")
                client.set_variable(node, name, [1] * size)
                value = client.get_variable(node, name, wait_ms=1000)
                print(f"Variable {name} = {value}")
        client.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:host=127.0.0.1;port=33333")
    args = parser.parse_args()
    main(args.target)
