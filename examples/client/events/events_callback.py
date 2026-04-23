import argparse
import logging
import sys
import time

from pyaseba import Client
from pyaseba.client import Event
from pyaseba.examples.utils import setup_logging


def cb(event: Event) -> None:
    logging.info(f"Received {event}")


script = """
onevent send
emit echo event.args[0:2]
"""

script = """
onevent send
emit echo args[0:2]
"""


def main(target: str) -> None:
    with Client() as client:
        client.add_event_callback(cb)
        if client.connect(target, max_retries=10):
            node_id, conn = client.wait_node(wait_ms=1000)
            if conn:
                client.load_script(node_id=node_id,
                                   script=script,
                                   events={
                                       "send": 3,
                                       "echo": 3
                                   })
                client.cmd_run(node_id=node_id)
                for i in range(5):
                    client.emit_event(node_id, "send", [3, 2, i])
                    time.sleep(0.5)
            else:
                raise RuntimeError("No node found!")
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
        logging.error(str(e))
        sys.exit(1)
