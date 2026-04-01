import argparse
import time

from pyaseba.client import connect


def main(target: str) -> None:
    with connect(target) as client:
        node = client.wait_node_connection(wait_ms=5000)
        if node:
            print(f'node {node}')
            desc = client.get_description(node)
            if desc:
                index, _ = desc._variables_map['motor.left.target']
                client.set_variables(node, index, [100, 100])
                client.set_variable(node, "leds.top", [0, 32, 0])
                while True:
                    data = client.get_variable(node, "prox.horizontal", wait_ms=1000)
                    if data is not None and data[2] > 2000:
                        break
                    time.sleep(0.1)
                client.reset(node=node)
                time.sleep(0.2)
        else:
            print('no node')
    time.sleep(0.2)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:host=127.0.0.1;port=33333")
    args = parser.parse_args()
    main(args.target)
