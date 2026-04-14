import argparse
import asyncio

from pyaseba import ClientAsync

script = """
onevent send
emit echo event.args[0:2]
"""

script = """
onevent send
emit echo args[0:2]
"""


async def main(target: str) -> None:
    client = ClientAsync()
    if await client.connect(target, max_retries=10):
        node_id, conn = await client.wait_node()
        assert conn
        client.load_script(node_id=node_id,
                           script=script,
                           events={"send": 3, "echo": 3})
        client.cmd_run(node_id=node_id)
        client.emit_event(node_id, "send", [3, 2, 1])
        e = await client.get_event(node_id, "echo")
        print(f"Got {e}")
        client.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:host=127.0.0.1;port=33333")
    args = parser.parse_args()
    asyncio.run(main(args.target))
