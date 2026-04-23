import argparse
import logging
import sys

from pyaseba import Client, print_description
from pyaseba.examples.utils import setup_logging

script = """
var a
var b
a = 0
b = 0
onevent c
a = b + 1
emit d [a, b]
"""


def main(target: str) -> None:
    with Client() as client:
        if client.connect(target, max_retries=10):
            node_id, conn = client.wait_node(wait_ms=1000)
            if conn:
                client.load_script(node_id=node_id,
                                   script=script,
                                   events={
                                       "c": 0,
                                       "d": 2
                                   })
                client.cmd_run(node_id)
                desc = client.get_description(node_id)
                assert desc
                print_description(node_id, desc)
                client.set_variable(node_id, "b", [10])
                client.emit_event(node_id, "c", [])
                e = client.get_event(node_id, "d", wait_ms=1000)
                if e:
                    logging.info(f"Got {e}")
                else:
                    raise RuntimeError("Received no event!")
                for v in ('a', 'b'):
                    value = client.get_variable(node_id, 'a', wait_ms=1000)
                    if len(value) == 1:
                        logging.info(f"{v} = {value}")
                    else:
                        raise RuntimeError(
                            f"Wrong variable size: {v} = {value}")
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
