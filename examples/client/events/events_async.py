import argparse
import asyncio

from pyaseba import ClientAsync

script = """
onevent send
emit echo event.args[0:2]
"""


async def main(target: str) -> None:
    client = ClientAsync()
    if await client.connect(target, max_retries=10):
        node = await client.wait_node_connection()
        client.load_script(node=node,
                            script=script,
                            events=[("send", 3), ("echo", 3)])
        client.run(node=node)
        client.emit_event(node, "send", [3, 2, 1])
        e = await client.get_event(node, "echo")
        print(f"Got {e}")
        client.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:host=127.0.0.1;port=33333")
    args = parser.parse_args()
    asyncio.run(main(args.target))
