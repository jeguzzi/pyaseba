import asyncio
import argparse
from pyaseba import connect_async


async def main(target: str) -> None:
    async with connect_async(target) as network:
        node = await network.wait_node_connection()
        if node:
            desc = network.get_description(node)
            assert(desc is not None)
            index, _ = desc._variables_map['motor.left.target']
            network.set_variables(node, index, [100, 100])
            network.set_variable(node, "leds.top", [0, 32, 0])
            while True:
                data = await network.get_variable(node, "prox.horizontal")
                if data[2] > 2000:
                    break
                await asyncio.sleep(0.1)
            network.reset(node=node)
            await asyncio.sleep(0.2)
    await asyncio.sleep(0.2)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:host=127.0.0.1;port=33333")
    args = parser.parse_args()
    asyncio.run(main(args.target))
