# skip
# Not working ... losing messages (while we send)

import argparse
import asyncio
import logging
import sys
from functools import partial

from pyaseba.client import ClientAsync
from pyaseba.examples.utils import setup_logging


def cb(conn: int, name: str, title: str = "") -> None:
    logging.info(f"{title} connection #{conn} to {name}")


async def main(target: str) -> None:
    with ClientAsync(port=33334, ping_period_ms=0,
                     automatic_query=False) as bridge:
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
    parser.add_argument('--log_level', default="INFO")
    args = parser.parse_args()
    setup_logging(args.log_level)
    try:
        asyncio.run(main(args.target))
    except KeyboardInterrupt:
        pass
    except Exception as e:
        logging.error(str(e))
        sys.exit(1)
