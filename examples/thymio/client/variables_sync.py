import argparse
import sys
import time

from pyaseba.client import Client


def main(target: str) -> None:
    client = Client()
    if client.connect(target):
        node_id, conn = client.wait_node(wait_ms=5000)
        if conn:
            desc = client.get_description(node_id)
            if desc and desc.name == 'thymio-II':
                print(f'Connected to Thymio {node_id}')
                index, _ = desc.variables['motor.left.target']
                client.set_variable_by_index(node_id, index, [100, 100])
                client.set_variable(node_id, "leds.top", [0, 32, 0])
                for _ in range(100):
                    data = client.get_variable(node_id,
                                               "prox.horizontal",
                                               wait_ms=1000)
                    if data and data[2] > 2000:
                        break
                    time.sleep(0.1)
                client.cmd_reset(node_id)
            else:
                raise RuntimeError(f"Node {node_id} is not a Thymio")
    else:
        raise RuntimeError(f"Could not connect to {target}!")


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="ser:name=Thymio")
    args = parser.parse_args()
    try:
        main(args.target)
    except Exception as e:
        sys.exit(f"ERROR: {e}")
