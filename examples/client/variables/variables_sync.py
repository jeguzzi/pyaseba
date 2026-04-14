import argparse

from pyaseba import Client


def main(target: str) -> None:
    client = Client()
    if client.connect(target, max_retries=10):
        node_id, conn = client.wait_node(wait_ms=5000)
        description = client.get_description(node_id)
        if description:
            (name, (_, size)), *_ = description.variables.items()
            value = client.get_variable(node_id, name, wait_ms=1000)
            print(f"Variable {name} = {value}")
            client.set_variable(node_id, name, [1] * size)
            value = client.get_variable(node_id, name, wait_ms=1000)
            print(f"Variable {name} = {value}")
        client.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:host=127.0.0.1;port=33333")
    args = parser.parse_args()
    main(args.target)
