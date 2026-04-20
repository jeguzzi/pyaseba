# skip

import argparse
import sys
from functools import partial
import time

from pyaseba.client import Client


def cb(conn: int, name: str, bridge: Client, title: str = "") -> None:
    print(f"{title} connection #{conn} to {name}")


def main(target: str) -> None:
    bridge = Client(port=33334, ping_period_ms=False, automatic_query=False)
    bridge.pause_processing = True
    if bridge.connect(target=target, max_retries=0):
        bridge.add_connection_callback(partial(cb, bridge=bridge, title="Added"))
        bridge.add_disconnection_callback(partial(cb, bridge=bridge, title="Removed"))
        while True:
            msg, conn = bridge.get_message(wait_ms=1000, pause=True)
            if msg and conn:
                bridge.send_message(msg, exclude={conn})


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:port=33333")
    args = parser.parse_args()
    try:
        main(args.target)
    except KeyboardInterrupt:
        pass
    except Exception as e:
        sys.exit(f"ERROR: {e}")
