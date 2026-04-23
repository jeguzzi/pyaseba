import argparse
import logging
import sys

from pyaseba import Client
from pyaseba.examples.utils import setup_logging


def main(target: str) -> None:
    with Client(port=33334) as client:
        if client.connect(target, max_retries=1):
            node_id, conn = client.wait_node(wait_ms=5000)
            if not conn:
                raise RuntimeError("No node found!")
            logging.info(
                f"All variables: {client.get_all_variables(node_id, wait_ms=2000)}"
            )
            description = client.get_description(node_id)
            if description:
                (name, (_, size)), *_ = description.variables.items()
                client.cmd_reset(node_id)
                value = client.get_variable(node_id, name, wait_ms=1000)
                logging.info(f"Variable {name} = {value}")
                client.set_variable(node_id, name, [1] * size)
                value = client.get_variable(node_id, name, wait_ms=1000)
                logging.info(f"Variable {name} = {value}")
            else:
                raise RuntimeError("No description!")
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
