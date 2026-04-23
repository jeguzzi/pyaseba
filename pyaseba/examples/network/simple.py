"""
Simple Network
"""

import argparse

from pyaseba.network import Network, Node


class SimpleNode(Node):

    events = {
        "event": "emitted at each control step after incrementing counter"
    }
    variables = {"counter": 1, "value": 1}
    functions = {
        "duplicate": ("duplicates the input", [("input", 1), ("result", 1)]),
        "square": ("set value to the square of the input", [("input", 1)])
    }

    def __init__(self, node_id: int, name: str) -> None:
        super().__init__(node_id, name=name, default_functions=False)

    def init(self) -> None:
        self.counter = 0
        self.value = 0
        self.set("_productId", [0])

    @property
    def counter(self) -> int:
        vs = self.get("counter")
        if vs:
            return vs[0]
        return 0

    @counter.setter
    def counter(self, value: int) -> None:
        self.set("counter", [value])

    @property
    def value(self) -> int:
        vs = self.get("value")
        if vs:
            return vs[0]
        return 0

    @value.setter
    def value(self, value: int) -> None:
        self.set("value", [value])

    def tick(self, time_step: float) -> None:
        self.counter += 1
        self.emit("event")

    def reset(self) -> None:
        self.counter = 0

    def duplicate(self, xs: list[int]) -> list[int]:
        print(f'Calling duplicate({xs})')
        return [x * 2 for x in xs]

    def square(self, xs: list[int]) -> None:
        print(f'Calling square({xs})')
        if xs:
            self.value = xs[0]**2


def main(number: int = 1, advertise: str = "pyaseba", name: str = "SimpleNode") -> None:
    network = Network(advertised_name=advertise)
    for i in range(max(1, number)):
        node = SimpleNode(i, name=name)
        network.add_node(node)
    try:
        network.spin(time_step=0.1, duration=-1)
    except Exception:
        pass


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--number', default=1, type=int)
    parser.add_argument('--advertise', default="pyaseba")
    parser.add_argument('--name', default="SimpleNode")
    args = parser.parse_args()
    main(args.number, args.advertise, args.name)
