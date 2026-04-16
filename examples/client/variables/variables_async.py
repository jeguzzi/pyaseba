import argparse
import asyncio
import sys

from pyaseba import ClientAsync


async def main(target: str) -> None:
    client = ClientAsync()
    if await client.connect(target, max_retries=1):
        node_id, conn = await client.wait_node()
        if not conn:
            raise RuntimeError("No node found!")
        description = client.get_description(node_id)
        if description:
            (name, (_, size)), *_ = description.variables.items()
            client.cmd_reset(node_id)
            value = await client.get_variable(node_id, name)
            print(f"Variable {name} = {value}")
            client.set_variable(node_id, name, [1] * size)
            value = await client.get_variable(node_id, name)
            print(f"Variable {name} = {value}")
        else:
            raise RuntimeError("No description!")
        client.close()
    else:
        raise RuntimeError(f"Could not connect to {target}!")


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:port=33333")
    args = parser.parse_args()
    try:
        asyncio.run(main(args.target))
    except Exception as e:
        sys.exit(f"ERROR: {e}")
