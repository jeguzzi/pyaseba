# skip

import argparse
import logging
import sys
import time
from functools import partial

from pyaseba.client import Client, Message
from pyaseba.examples.utils import setup_logging


def forward(msg: Message, conn: int, bridge: Client) -> None:
    bridge.send_message(msg, exclude={conn})


def cb(conn: int, name: str, title: str = "") -> None:
    logging.info(f"{title} connection #{conn} to {name}")


def main(target: str) -> None:
    bridge = Client(port=33334, ping_period_ms=False, automatic_query=False)
    bridge.add_message_callback(partial(forward, bridge=bridge))
    if bridge.connect(target, max_retries=0):
        bridge.add_connection_callback(partial(cb, title="Added"))
        bridge.add_disconnection_callback(partial(cb, title="Removed"))
        while True:
            try:
                time.sleep(1)
            except KeyboardInterrupt:
                break
        bridge.close()
    else:
        raise RuntimeError(f"Could not connect to {target}!")


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:port=33333")
    parser.add_argument('--log_level', default="INFO")
    args = parser.parse_args()
    setup_logging(args.log_level)
    try:
        main(args.target)
    except Exception as e:
        sys.exit(f"ERROR: {e}")
