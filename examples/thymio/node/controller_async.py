import argparse
import asyncio
import sys
from functools import partial

from pyaseba.client.thymio import ThymioAsync


def control(thymio: ThymioAsync, dt: float, loop: asyncio.AbstractEventLoop,
            done: asyncio.Future[None]) -> None:
    if thymio.prox_horizontal[2] > 2000:
        thymio.motor_left_target = 0
        thymio.motor_right_target = 0
        thymio.leds_top = [0, 0, 0]
        if not done.done():
            loop.call_soon_threadsafe(done.set_result, None)


async def main(target: str) -> None:
    thymio = ThymioAsync()
    if await thymio.connect(target=target):
        thymio.leds_top = [0, 32, 0]
        thymio.motor_left_target = 100
        thymio.motor_right_target = 100
        loop = asyncio.get_running_loop()
        done = loop.create_future()
        thymio.set_controller(partial(control, loop=loop, done=done),
                              event="prox")
        await done
    else:
        raise RuntimeError(f'Could not find a Thymio on {target}')
    await thymio.close(reset=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="ser:name=Thymio")
    args = parser.parse_args()
    try:
        asyncio.run(main(args.target))
    except Exception as e:
        sys.exit(f"ERROR: {e}")
