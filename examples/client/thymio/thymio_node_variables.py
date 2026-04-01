import argparse
from pyaseba.client.thymio import Thymio
import time

def main(target: str) -> None:
    node = Thymio()
    if node.connect(target=target):
        print(f'Node {node._node_id} variables:')
        print(list(node._variable_values))
        for _ in range(5):
            for k, v in node._variable_values.items():
                if v is not None:
                    value: int | list[int]
                    if len(v) == 1:
                        value = v[0]
                    else:
                        value = v
                    print(f'- {k}: {value}')
            time.sleep(1)
            print('-' * 50)
    else:
        print(f'Could not find a Thymio on {target}')
    print('Will close')
    node.close(reset=True)
    print('Closed')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:host=127.0.0.1;port=33333")
    args = parser.parse_args()
    main(args.target)
