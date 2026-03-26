import asyncio
import argparse
from pyaseba import connect_async, Event


async def main(target: str) -> None:
    async with connect_async(target) as network:
        node = await network.wait_node_connection()
        if node:
            loop = asyncio.get_running_loop()
            done = loop.create_future()
            script = """
onevent prox
emit proxh prox.horizontal
"""
            network.load_script(node=node,
                                script=script,
                                events=[("proxh", 7)])
            desc = network.get_description(node)
            assert(desc is not None)
            index, _ = desc._variables_map['motor.left.target']

            def cb(event: Event) -> None:
                if event.name == 'proxh' and event.source == node:
                    if event.data[2] > 2000:
                        if not done.done():
                            loop.call_soon_threadsafe(done.set_result, True)

            network.add_event_callback(cb)
            network.run(node=node)
            network.set_variables(node, index, [100, 100])
            network.set_variable(node, "leds.top", [0, 32, 0])
            await done
            network.reset(node=node)
            await asyncio.sleep(0.2)
    await asyncio.sleep(0.2)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:host=127.0.0.1;port=33333")
    args = parser.parse_args()
    asyncio.run(main(args.target))
