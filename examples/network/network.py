# import time

from pyaseba.network import Network, Node


class MyNode(Node):

    events = ["event"]
    variables = [("value", 3), ("counter", 1)]
    functions = [("duplicate", [(3, "input"), (3, "result")])]

    def __init__(self, node_id: int):
        Node.__init__(self,
                      node_id,
                      name="MyNode",
                      default_functions=False,
                      advertised_name="Advertised Node")

    def init(self) -> None:
        self.counter = 0
        self.set("_productId", [9])

    @property
    def counter(self) -> int:
        return self._c

    @counter.setter
    def counter(self, value: int) -> None:
        self._c = value
        self.set("counter", [value])

    def tick(self, time_step: float) -> None:
        print('tick', time_step)
        self.counter += 1
        self.emit("event")

    def reset(self) -> None:
        self.counter = 0

    def duplicate(self, xs: list[int]) -> list[int]:
        return [x * 2 for x in xs]


def main() -> None:
    network = Network(port=10000)
    for i in range(2):
        node = MyNode(i)
        network.add_node(node)
    network.spin(time_step=0.1, duration=10)

    # for _ in range(6000):
    #     server.spin(0.1)
    #     time.sleep(0.1)


if __name__ == '__main__':
    main()
