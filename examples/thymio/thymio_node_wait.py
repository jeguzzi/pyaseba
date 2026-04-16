import argparse
import asyncio

from pyaseba.client.thymio import ThymioAsync


async def main(target: str) -> None:
    thymio = ThymioAsync()
    if await thymio.connect(target=target):
        print(f'prox.horizontal: {thymio.prox_horizontal}')
        print('waiting first prox event')
        await thymio.wait("prox")
        print(f'prox.horizontal: {thymio.prox_horizontal}')
    else:
        print(f'Could not find a Thymio on {target}')
    await thymio.close(reset=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:port=33333")
    args = parser.parse_args()
    asyncio.run(main(args.target))
