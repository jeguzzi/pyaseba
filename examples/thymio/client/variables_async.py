import argparse
import sys
import asyncio

from pyaseba.client import ClientAsync


async def main(target: str) -> None:
    client = ClientAsync()
    if await client.connect(target):
        node_id, conn = await client.wait_node(wait_ms=5000)
        if conn:
            desc = client.get_description(node_id)
            if desc and desc.name == 'thymio-II':
                print(f'Connected to Thymio {node_id}')
                index, _ = desc.variables['motor.left.target']
                client.set_variable_by_index(node_id, index, [100, 100])
                client.set_variable(node_id, "leds.top", [0, 32, 0])
                for _ in range(100):
                    data = await client.get_variable(node_id,
                                                     "prox.horizontal",
                                                     wait_ms=1000)
                    if data and data[2] > 2000:
                        break
                    await asyncio.sleep(0.1)
                client.cmd_reset(node_id)
            else:
                raise RuntimeError(f"Node {node_id} is not a Thymio")
    else:
        raise RuntimeError(f"Could not connect to {target}!")


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="ser:name=Thymio")
    args = parser.parse_args()
    try:
        asyncio.run(main(args.target))
    except Exception as e:
        sys.exit(f"ERROR: {e}")
