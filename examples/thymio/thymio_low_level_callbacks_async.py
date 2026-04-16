import asyncio
import argparse
from pyaseba.client import ClientAsync, Event


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
            assert (desc is not None)
            index, _ = desc.variables['motor.left.target']

            def cb(event: Event) -> None:
                if event.name == 'proxh' and event.source == node_id:
                    if event.data[2] > 2000:
                        if not done.done():
                            loop.call_soon_threadsafe(done.set_result, True)

            client.add_event_callback(cb)
            client.cmd_run(node_id)
            client.set_variable_by_index(node_id, index, [100, 100])
            client.set_variable(node_id, "leds.top", [0, 32, 0])
            await done
            client.cmd_reset(node_id)
            await asyncio.sleep(0.2)
    await asyncio.sleep(0.2)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:port=33333")
    args = parser.parse_args()
    asyncio.run(main(args.target))
