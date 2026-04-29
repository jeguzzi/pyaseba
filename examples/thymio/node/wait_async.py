import argparse
import asyncio
import logging
import sys

from pyaseba.client.thymio import ThymioAsync
from pyaseba.examples.utils import setup_logging


async def main(target: str) -> None:
    thymio = ThymioAsync()
    if await thymio.connect(target=target, wait_ms=1000, start_mirroring=True):
        thymio.leds_top = [32, 32, 0]
        thymio.motor_left_target = 100
        thymio.motor_right_target = 100
        thymio.sync()
        while True:
            await thymio.wait("prox", wait_ms=200)
            if thymio.prox_horizontal[2] > 2000:
                break
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
