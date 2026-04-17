import argparse
import sys
import asyncio

from pyaseba.client import NodeAsync, EventSpec


class SimpleNode(NodeAsync):
    events = {"event": EventSpec(variables=["counter"])}
    properties = ['value', 'counter']
    counter: int
    value: int


async def main(target: str) -> None:
    node = SimpleNode()
    if await node.connect(target=target):
        print(f"Variables: {await node.get_all()}")
        for name in ('value', 'counter'):
            print(f"{name} = {await node.get(name)}")
            node.set(name, 1, cached=False)
            print(f"Set {name} to {await node.get(name, cached=False)}")
        # Variables as attributes
        print(f"We can access variables in {node.properties} as attibutes")
        print(f"value = {node.value}")
        print(f"counter = {node.counter}")
        # Events
        print(f"We can wait for local events in {node.mirrored_events}")
        for _ in range(5):
            await node.wait("event", wait_ms=1000)
            print(f"counter = {node.counter}")
        # Functions
        print(f"We can call functions in {node.exposed_functions}")
        print('Calling square(11) should set value = 121')
        node.call("square", 11)
        print(f"value = {await node.get("value", cached=False)}")
        await node.close()
    else:
        raise RuntimeError(f"Could not connect node on {target}!")


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:port=33333")
    args = parser.parse_args()
    try:
        asyncio.run(main(args.target))
    except Exception as e:
        sys.exit(f"ERROR: {e}")
