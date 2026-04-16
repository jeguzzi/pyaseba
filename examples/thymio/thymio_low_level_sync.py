import argparse
import time

from pyaseba.client import Client


def main(target: str) -> None:
    client = Client()
    if client.connect(target):
        node_id, conn = client.wait_node(wait_ms=5000)
        if conn:
            print(f'node {node_id}')
            desc = client.get_description(node_id)
            if desc:
                index, _ = desc.variables['motor.left.target']
                client.set_variable_by_index(node_id, index, [100, 100])
                client.set_variable(node_id, "leds.top", [0, 32, 0])
                while True:
                    data = client.get_variable(node_id,
                                               "prox.horizontal",
                                               wait_ms=1000)
                    if data is not None and data[2] > 2000:
                        break
                    time.sleep(0.1)
                client.cmd_reset(node_id)
                time.sleep(0.2)
        else:
            print('no node')
    time.sleep(0.2)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:port=33333")
    args = parser.parse_args()
    main(args.target)
