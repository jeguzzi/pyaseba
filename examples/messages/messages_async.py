import argparse
import asyncio

from pyaseba import NetworkAsync


async def main(target: str) -> None:
    network = NetworkAsync()
    if await network.connect(target, max_retries=10):
        for _ in range(10):
            msg = await network.get_message()
            print(f"Got message {msg}")
        network.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:host=127.0.0.1;port=33333")
    args = parser.parse_args()
    asyncio.run(main(args.target))
