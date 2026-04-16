import asyncio
import argparse
from pyaseba.client import ClientAsync


async def main(target: str) -> None:
    client = ClientAsync()
    if await client.connect(target):
        node_id, conn = await client.wait_node()
        if conn:
            desc = client.get_description(node_id)
            assert desc
            index, _ = desc.variables['motor.left.target']
            client.set_variable_by_index(node_id, index, [100, 100])
            client.set_variable(node_id, "leds.top", [0, 32, 0])
            while True:
                data = await client.get_variable(node_id, "prox.horizontal")
                if data[2] > 2000:
                    break
                await asyncio.sleep(0.1)
            client.cmd_reset(node_id)
            await asyncio.sleep(0.2)
    await asyncio.sleep(0.2)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:port=33333")
    args = parser.parse_args()
    asyncio.run(main(args.target))
