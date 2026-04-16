import argparse
import asyncio
import sys

from pyaseba import ClientAsync


async def main(target: str) -> None:
    client = ClientAsync()
    if await client.connect(target, max_retries=10):
        for _ in range(5):
            msg, conn = await client.get_message()
            print(f"Received {msg} from #{conn}")
        client.close()
    else:
        raise RuntimeError(f"Could not connect to {target}!")


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:port=33333")
    args = parser.parse_args()
    try:
        asyncio.run(main(args.target))
    except Exception as e:
        sys.exit(f"ERROR: {e}")
