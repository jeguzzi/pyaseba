# import time

from pyaseba.network import Network, Node


class MyNode(Node):

    events = {
        "event": "emitted at each control step after incrementing counter"
    }
    variables = {"counter": 1, "value": 3}
    functions = {
        "duplicate": ("duplicates the input", [("input", 3), ("result", 3)]),
        "square": ("sets the square of the input to value", [("input", 3)])
    }

    def __init__(self, node_id: int):
        Node.__init__(self,
                      node_id,
                      name="MyNode",
                      default_functions=False,
                      advertised_name="Advertised Node")

    def init(self) -> None:
        print('MyNode.init')
        self.counter = 0
        self.set("_productId", [9])

    @property
    def counter(self) -> int:
        vs = self.get("counter")
        if vs:
            return vs[0]
        return 0

    @counter.setter
    def counter(self, value: int) -> None:
        self._c = value
        self.set("counter", [value])

    def tick(self, time_step: float) -> None:
        print(f'MyNode.tick({time_step:.1f})')
        self.counter += 1
        self.emit("event")

    def reset(self) -> None:
        print('MyNode.reset')
        self.counter = 0

    def duplicate(self, xs: list[int]) -> list[int]:
        return [x * 2 for x in xs]


def main() -> None:
    network = Network(port=10000)
    for i in range(2):
        node = MyNode(i)
        network.add_node(node)
    network.spin(time_step=0.1, duration=10)


if __name__ == '__main__':
    main()
