import argparse
import asyncio

from pyaseba import NetworkAsync


async def main(target: str) -> None:
    network = NetworkAsync()
    if await network.connect(target, max_retries=10):
        node = await network.wait_node_connection()
        if node is not None:
            description = network.get_description(node)
            if description:
                name, size = description.variables[0]
                value = await network.get_variable(node, name)
                print(f"Variable {name} = {value}")
                network.set_variable(node, name, [1] * size)
                value = await network.get_variable(node, name)
                print(f"Variable {name} = {value}")
        network.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:host=127.0.0.1;port=33333")
    args = parser.parse_args()
    asyncio.run(main(args.target))
