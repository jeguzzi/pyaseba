import argparse
import sys

from pyaseba.client import EventSpec, MirroringConfig, Node


class SimpleNode(Node):
    mirroring_config = MirroringConfig(
        events={"event": EventSpec(variables=["counter"])},
        function_include=[r'.*', ])
    properties = ['value', 'counter']
    counter: int
    value: int


def main(target: str) -> None:
    node = SimpleNode()
    if node.connect(target=target, start_mirroring=True):
        # Variables
        print(f"Variables: {node.get_all()}")
        for name in ('value', 'counter'):
            print(f"{name} = {node.get(name)}")
            node.set(name, 1, cached=False)
            print(f"Set {name} to {node.get(name, cached=False)}")
        # Variables as attributes
        print(f"We can access variables in {node.properties} as attibutes")
        print(f"value = {node.value}")
        print(f"counter = {node.counter}")
        # Events
        print(f"We can wait for local events in {node.mirrored_events}")
        for _ in range(5):
            node.wait("event", wait_ms=1000)
            print(f"counter = {node.counter}")
        # Functions
        print(f"We can call functions in {node.mirrored_functions}")
        print('Calling square(11) should set value = 121')
        node.call("square", 11)
        print(f"value = {node.get("value", cached=False)}")
        node.close(reset=True)
    else:
        raise RuntimeError(f"Could not connect node on {target}!")


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:port=33333")
    args = parser.parse_args()
    try:
        main(args.target)
    except Exception as e:
        sys.exit(f"ERROR: {e}")
