import time

from pyaseba.server import Server, Node


class MyNode(Node):

    events = ["event"]
    variables = [("value", 3), ("counter", 1)]
    functions = [("duplicate", [(3, "input"), (3, "result")])]

    def __init__(self, node_id: int, name: str):
        Node.__init__(self, node_id, name)
        self.counter = 0

    @property
    def counter(self) -> int:
        return self._c

    @counter.setter
    def counter(self, value: int) -> None:
        self._c = value
        self.set("counter", [value])

    def tick(self, time_step: float) -> None:
        # self.counter += 1
        self.emit("event")

    def reset(self) -> None:
        self.counter = 0

    def duplicate(self, xs: list[int]) -> list[int]:
        return [x * 2 for x in xs]


def main() -> None:
    server = Server()
    for i in range(2):
        node = MyNode(i, "MyNode")
        server.add_node(node)
    for _ in range(6000):
        server.spin(0.1)
        time.sleep(0.1)


if __name__ == '__main__':
    main()
