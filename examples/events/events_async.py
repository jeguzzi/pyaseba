import argparse
import asyncio

from pyaseba import NetworkAsync

script = """
onevent send
emit echo event.args[0:2]
"""


async def main(target: str) -> None:
    network = NetworkAsync()
    if await network.connect(target, max_retries=10):
        node = await network.wait_node_connection()
        network.load_script(node=node,
                            script=script,
                            events=[("send", 3), ("echo", 3)])
        network.run(node=node)
        network.emit_event(node, "send", [3, 2, 1])
        e = await network.get_event(node, "echo")
        print(f"Got {e}")
        network.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:host=127.0.0.1;port=33333")
    args = parser.parse_args()
    asyncio.run(main(args.target))
