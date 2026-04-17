import argparse
import asyncio
import sys

from pyaseba.client.thymio import ThymioAsync


async def main(target: str) -> None:
    thymio = ThymioAsync()
    if await thymio.connect(target=target, wait_ms=1000):
        thymio.leds_top = [32, 32, 0]
        thymio.motor_left_target = 100
        thymio.motor_right_target = 100
        thymio.sync()
        while True:
            await thymio.wait("prox", wait_ms=200)
            if thymio.prox_horizontal[2] > 2000:
                break
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
