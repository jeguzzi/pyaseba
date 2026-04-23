import argparse
import logging
import sys
import time

from pyaseba.client import Client, Event
from pyaseba.examples.utils import setup_logging

script = """
onevent prox
emit proxh prox.horizontal
"""


def main(target: str) -> None:
    with Client() as client:
        if client.connect(target):
            node_id, conn = client.wait_node(wait_ms=5000)
            if conn:
                done = False
                client.load_script(node_id=node_id,
                                   script=script,
                                   events={"proxh": 7})
                desc = client.get_description(node_id)
                assert desc
                index, _ = desc.variables['motor.left.target']
                client.set_variable_by_index(node_id, index, [100, 100])
                client.set_variable(node_id, "leds.top", [0, 32, 0])

                def cb(event: Event) -> None:
                    nonlocal done
                    if event.name == 'proxh' and event.source == node_id:
                        if event.data[2] > 2000:
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
