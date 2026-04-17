import argparse
import sys

from pyaseba import Client


def main(target: str) -> None:
    client = Client()
    if client.connect(target, max_retries=1):
        node_id, conn = client.wait_node(wait_ms=5000)
        if not conn:
            raise RuntimeError("No node found!")
        print(
            f"All variables: {client.get_all_variables(node_id, wait_ms=2000)}"
        )
        description = client.get_description(node_id)
        if description:
            (name, (_, size)), *_ = description.variables.items()
            client.cmd_reset(node_id)
            value = client.get_variable(node_id, name, wait_ms=1000)
            print(f"Variable {name} = {value}")
            client.set_variable(node_id, name, [1] * size)
            value = client.get_variable(node_id, name, wait_ms=1000)
            print(f"Variable {name} = {value}")
        else:
            raise RuntimeError("No description!")
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
