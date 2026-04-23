import argparse
import asyncio
import logging
import sys
from functools import partial

from pyaseba.client.thymio import ThymioAsync
from pyaseba.examples.utils import setup_logging


def control(thymio: ThymioAsync, loop: asyncio.AbstractEventLoop,
            done: asyncio.Future[None]) -> None:
    if any(v < 100 for v in thymio.prox_ground_delta):
        thymio.motor_left_target = 0
        thymio.motor_right_target = 0
        thymio.leds_top = [32, 0, 0]
        if not done.done():
            loop.call_soon_threadsafe(done.set_result, None)
        thymio.sync()


class Switch:

    def __init__(self) -> None:
        self.moving = False

    def __call__(self, thymio: ThymioAsync) -> None:
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


async def main(target: str) -> None:
    thymio = ThymioAsync()
    loop = asyncio.get_running_loop()
    done = loop.create_future()
    thymio.set_callback("prox", partial(control, loop=loop, done=done))
    thymio.set_callback("button.forward", Switch())
    if await thymio.connect(target=target):
        await done
        thymio.call_leds_buttons(0, 0, 0, 0)
    else:
        logging.error(f'Could not find a Thymio on {target}')
    await thymio.close(reset=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="ser:name=Thymio")
    parser.add_argument('--log_level', default="INFO")
    args = parser.parse_args()
    setup_logging(args.log_level)
    try:
        asyncio.run(main(args.target))
    except Exception as e:
        logging.error(str(e))
        sys.exit(1)
