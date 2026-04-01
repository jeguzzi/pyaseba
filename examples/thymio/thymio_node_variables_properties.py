import argparse
from pyaseba.client.thymio import Thymio
import time


def main(target: str) -> None:
    node = Thymio()
    if node.connect(target=target):
        for i in range(5):
            print(node.prox_horizontal)
            node.leds_top = [4 * i, 0, 32 - 4 * i]
            node.sync()
            time.sleep(1)

    else:
        print(f'Could not find a Thymio on {target}')
    node.close(reset=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:host=127.0.0.1;port=33333")
    args = parser.parse_args()
    main(args.target)
