"""
Simple Network
"""

from pyaseba.network import Network, Node


class SimpleNode(Node):

    events = ["event"]
    variables = [("counter", 1), ("value", 1)]
    functions = [
        ("duplicate", [(1, "input"), (1, "result")]),
        ("square", [(1, "input")]),
    ]

    def __init__(self, node_id: int) -> None:
        super().__init__(node_id, name="SimpleNode", default_functions=False)

    def init(self) -> None:
        self.counter = 0
        self.value = 0
        self.set("_productId", [0])

    @property
    def counter(self) -> int:
        return self._c

    @counter.setter
    def counter(self, value: int) -> None:
        self._c = value
        self.set("counter", [value])

    @property
    def value(self) -> int:
        return self._value

    @value.setter
    def value(self, value: int) -> None:
        self._value = value
        self.set("value", [value])

    def tick(self, time_step: float) -> None:
        self.counter += 1
        self.emit("event")

    def reset(self) -> None:
        self.counter = 0

    def duplicate(self, xs: list[int]) -> list[int]:
        return [x * 2 for x in xs]

    def square(self, xs: list[int]) -> None:
        if xs:
            self.value = xs[0] ** 2


def main() -> None:
    network = Network()
    node = SimpleNode(0)
    network.add_node(node)
    try:
        network.spin(time_step=0.1, duration=-1)
    except Exception:
        pass


if __name__ == '__main__':
    main()
