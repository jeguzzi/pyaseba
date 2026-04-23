import argparse
import asyncio
import logging
import sys

from pyaseba.client import ClientAsync, Event
from pyaseba.examples.utils import setup_logging

script = """
motor.left.target = 100
motor.right.target = 100
leds.top = [0, 32, 0]
onevent prox
if prox.horizontal[2] > 2000 then
  emit done
end
"""


async def main(target: str) -> None:
    with ClientAsync() as client:
        if await client.connect(target):
            node_id, conn = await client.wait_node(wait_ms=5000)
            if conn:
                client.load_script(node_id=node_id,
                                   script=script,
                                   events={"done": 0})
                loop = asyncio.get_running_loop()
                done = loop.create_future()

                def cb(event: Event) -> None:
                    if (not done.done() and event.name == 'done'
                            and event.source == node_id):
                        loop.call_soon_threadsafe(done.set_result, None)

                client.add_event_callback(cb)
                client.cmd_run(node_id)
                await done
                client.cmd_reset(node_id)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="ser:name=Thymio")
    parser.add_argument('--log_level', default="INFO")
    args = parser.parse_args()
    setup_logging(args.log_level)
    try:
        asyncio.run(main(args.target))
    except Exception as e:
        logging.error(str(e))
        sys.exit(1)
