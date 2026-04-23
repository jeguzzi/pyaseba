import argparse
import asyncio
import logging
import sys

from pyaseba import ClientAsync
from pyaseba.examples.utils import setup_logging


async def main(target: str) -> None:
    with ClientAsync() as client:
        if await client.connect(target, max_retries=1):
            node_id, conn = await client.wait_node()
            if not conn:
                raise RuntimeError("No node found!")
            logging.info(
                f"All variables: {await client.get_all_variables(node_id, wait_ms=2000)}"
            )
            description = client.get_description(node_id)
            if description:
                (name, (_, size)), *_ = description.variables.items()
                client.cmd_reset(node_id)
                value = await client.get_variable(node_id, name)
                logging.info(f"Variable {name} = {value}")
                client.set_variable(node_id, name, [1] * size)
                value = await client.get_variable(node_id, name)
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
        asyncio.run(main(args.target))
    except Exception as e:
        logging.error(str(e))
        sys.exit(1)
