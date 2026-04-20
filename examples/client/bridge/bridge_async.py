# skip
# Not working ... losing messages (while we send)

import argparse
import asyncio
import sys
from functools import partial

from pyaseba.client import ClientAsync


def cb(conn: int, name: str, title: str = "") -> None:
    print(f"{title} connection #{conn} to {name}")


async def main(target: str) -> None:
    bridge = ClientAsync(port=33334,
                         ping_period_ms=False,
                         automatic_query=False)
    bridge.pause_processing = False
    if await bridge.connect(target=target, max_retries=0):
        bridge.add_connection_callback(partial(cb, title="Added"))
        bridge.add_disconnection_callback(partial(cb, title="Removed"))
        while True:
            msg, conn = await bridge.get_message(wait_ms=1000, pause=True)
            if msg and conn:
                bridge.send_message(msg, exclude={conn})


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:port=33333")
    args = parser.parse_args()
    try:
        asyncio.run(main(args.target))
    except KeyboardInterrupt:
        pass
    except Exception as e:
        sys.exit(f"ERROR: {e}")
