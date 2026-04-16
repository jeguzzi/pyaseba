import argparse
import time
from collections.abc import Callable

from pyaseba.client.node import Node
from pyaseba.client.thymio import Thymio

done = False


def control(node: Thymio) -> None:
    global done
    values = node.prox_ground_delta
    if any(v < 100 for v in values):
        node.motor_left_target = 0
        node.motor_right_target = 0
        node.leds_top = [32, 0, 0]
        if not done:
            done = True


def switch() -> Callable[[Thymio], None]:
    moving = False

    def f(node: Thymio) -> None:
        nonlocal moving
        if not node.button_forward:
            if moving:
                print('Button -> Stop')
                node.motor_left_target = 0
                node.motor_right_target = 0
                node.leds_top = [0, 0, 0]
                node.call_leds_buttons(0, 0, 0, 0)
            else:
                print('Button -> Start')
                node.motor_left_target = 100
                node.motor_right_target = 100
                node.leds_top = [32, 32, 0]
                node.call_leds_buttons(32, 0, 0, 0)
            moving = not moving
    return f


def main(target: str) -> None:
    node = Thymio()
    node.on_prox = control
    node.on_button_forward = switch()
    if node.connect(target=target):
        print(list(node._functions))
        while not done:
            time.sleep(0.1)
            node.sync()
        time.sleep(0.5)
        node.call_leds_buttons(0, 0, 0, 0)
        node.sync()
    else:
        print(f'Could not find a Thymio on {target}')
    node.close(reset=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:port=33333")
    args = parser.parse_args()
    main(args.target)
