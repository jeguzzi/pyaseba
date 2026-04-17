import argparse
import asyncio
import sys
from functools import partial

from pyaseba.client import ClientAsync, Event


def cb(event: Event, loop: asyncio.AbstractEventLoop, node_id: int, name: str,
       done: asyncio.Future[None]) -> None:
    if event.name == name and event.source == node_id:
        if event.data[2] > 2000:
            if not done.done():
                loop.call_soon_threadsafe(done.set_result, None)


async def main(target: str) -> None:
    client = ClientAsync()
    if await client.connect(target):
        node_id, conn = await client.wait_node()
        if conn:
            loop = asyncio.get_running_loop()
            done = loop.create_future()
            script = """
onevent prox
emit proxh prox.horizontal
"""
            client.load_script(node_id=node_id,
                               script=script,
                               events={"proxh": 7})
            desc = client.get_description(node_id)
            assert desc
            index, _ = desc.variables['motor.left.target']
            client.set_variable_by_index(node_id, index, [100, 100])
            client.set_variable(node_id, "leds.top", [0, 32, 0])
            client.add_event_callback(
                partial(cb,
                        loop=loop,
                        name='proxh',
                        node_id=node_id,
                        done=done))
            client.cmd_run(node_id)
            await done
            client.cmd_reset(node_id)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="ser:name=Thymio")
    args = parser.parse_args()
    try:
        asyncio.run(main(args.target))
    except Exception as e:
        sys.exit(f"ERROR: {e}")
