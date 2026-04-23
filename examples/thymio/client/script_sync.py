import argparse
import logging
import sys
import time

from pyaseba.client import Client, Event
from pyaseba.examples.utils import setup_logging

script = """
motor.left.target = 100
motor.right.target = 100
leds.top = [0, 32, 0]
onevent prox
if prox.horizontal[2] > 2000 then
  emit done
end
"""


def main(target: str) -> None:
    with Client() as client:
        if client.connect(target):
            node_id, conn = client.wait_node(wait_ms=5000)
            if conn:
                done = False
                client.load_script(node_id=node_id,
                                   script=script,
                                   events={"done": 0})

                def cb(event: Event) -> None:
                    nonlocal done
                    if event.name == 'done' and event.source == node_id:
                        done = True

                client.add_event_callback(cb)
                client.cmd_run(node_id)
                while not done:
                    time.sleep(0.1)
                client.cmd_reset(node_id)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="ser:name=Thymio")
    parser.add_argument('--log_level', default="INFO")
    args = parser.parse_args()
    setup_logging(args.log_level)
    try:
        main(args.target)
    except Exception as e:
        logging.error(str(e))
        sys.exit(1)
