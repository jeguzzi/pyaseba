import argparse
import time
import sys

from pyaseba.client.thymio import Thymio

done = False


def control(thymio: Thymio, dt: float) -> None:
    global done
    if thymio.prox_horizontal[2] > 2000:
        done = True
        thymio.motor_left_target = 0
        thymio.motor_right_target = 0
        thymio.leds_top = [0, 0, 0]


def main(target: str) -> None:
    thymio = Thymio()
    if thymio.connect(target=target):
        thymio.leds_top = [0, 32, 0]
        thymio.motor_left_target = 100
        thymio.motor_right_target = 100
        thymio.set_controller(control, event="prox")
        try:
            while not done:
                time.sleep(1)
        except KeyboardInterrupt:
            pass
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
