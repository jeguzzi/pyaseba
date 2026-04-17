import argparse
import sys

from pyaseba.client.thymio import Thymio


def main(target: str) -> None:
    thymio = Thymio()
    if thymio.connect(target=target):
        thymio.leds_top = [32, 32, 0]
        thymio.motor_left_target = 100
        thymio.motor_right_target = 100
        thymio.sync()
        while True:
            thymio.wait("prox")
            if thymio.prox_horizontal[2] > 2000:
                break
    else:
        raise RuntimeError(f'Could not find a Thymio on {target}')
    thymio.close(reset=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="ser:name=Thymio")
    args = parser.parse_args()
    try:
        main(args.target)
    except Exception as e:
        sys.exit(f"ERROR: {e}")
