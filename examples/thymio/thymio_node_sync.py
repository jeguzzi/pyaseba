import argparse
import time
import typing
from collections.abc import Callable

from pyaseba.node import Node
from pyaseba.thymio import Thymio

done = False


def control(node: Node) -> None:
    global done
    values = typing.cast('list[int]', node.get("prox.ground.delta",
                                               cached=True))
    if any(v < 100 for v in values):
        node.set("motor.left.target", 0)
        node.set("motor.right.target", 0)
        node.set("leds.top", [32, 0, 0])
        if not done:
            done = True


def switch() -> Callable[[Node], None]:
    moving = False

    def f(node: Node) -> None:
        nonlocal moving
        if not node.get("button.forward", cached=True):
            if moving:
                print('Button -> Stop')
                node.set("motor.left.target", 0)
                node.set("motor.right.target", 0)
                node.set("leds.top", [0, 0, 0])
                node.call("leds.buttons", 0, 0, 0, 0)
            else:
                print('Button -> Start')
                node.set("motor.left.target", 100)
                node.set("motor.right.target", 100)
                node.set("leds.top", [32, 32, 0])
                node.call("leds.buttons", 32, 0, 0, 0)
            moving = not moving

    return f


def main(target: str) -> None:
    node = Thymio()
    node.set_callback("prox", control)
    node.set_callback("button.forward", switch())
    if node.connect(target=target):
        while not done:
            time.sleep(0.1)
        time.sleep(0.5)
        node.call("leds.buttons", 0, 0, 0, 0)
    else:
        print(f'Could not find a Thymio on {target}')
    node.close(reset=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:host=127.0.0.1;port=33333")
    args = parser.parse_args()
    main(args.target)
