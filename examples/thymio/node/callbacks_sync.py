import argparse
import sys
import time

from pyaseba.client.thymio import Thymio


class Control:

    def __init__(self) -> None:
        self.done = False

    def __call__(self, thymio: Thymio) -> None:
        if any(v < 100 for v in thymio.prox_ground_delta):
            thymio.motor_left_target = 0
            thymio.motor_right_target = 0
            thymio.leds_top = [32, 0, 0]
            thymio.sync()
            self.done = True


class Switch:

    def __init__(self) -> None:
        self.moving = False

    def __call__(self, thymio: Thymio) -> None:
        if thymio.button_forward:
            if self.moving:
                thymio.motor_left_target = 0
                thymio.motor_right_target = 0
                thymio.leds_top = [0, 0, 0]
                thymio.call_leds_buttons(0, 0, 0, 0)
            else:
                thymio.motor_left_target = 100
                thymio.motor_right_target = 100
                thymio.leds_top = [32, 32, 0]
                thymio.call_leds_buttons(32, 0, 0, 0)
            self.moving = not self.moving
            thymio.sync()


def main(target: str) -> None:
    thymio = Thymio()
    control = Control()
    thymio.set_callback("prox", control)
    thymio.set_callback("button.forward", Switch())
    if thymio.connect(target=target):
        while not control.done:
            time.sleep(1)
        thymio.call_leds_buttons(0, 0, 0, 0)
    else:
        print(f'Could not find a Thymio on {target}')
    thymio.close(reset=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="ser:name=Thymio")
    args = parser.parse_args()
    try:
        main(args.target)
    except Exception as e:
        sys.exit(f"ERROR: {e}")
