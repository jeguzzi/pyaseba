"""
Network
=======

Illustrative example of how to setup a simple Aseba network
from Python.
"""

# %%
# The network will be reachable at target tcp:port=10000
# from the local network.

from pyaseba.network import Network, Node

network = Network(port=10000)
network

# %%
# Nodes are specializes the :py:class:pyaseba.network.Node`
# base class, by adding events, variables, functions and (periodic)
# control step callbacks.


class SimpleNode(Node):

    events = {
        "event": "emitted at each control step after incrementing counter"
    }
    variables = {"counter": 1}
    functions = {
        "duplicate": ("duplicates the input", [("input", 1), ("result", 1)])
    }

    def __init__(self, node_id: int):
        super().__init__(node_id, name="SimpleNode", default_functions=False)

    def init(self) -> None:
        self.counter = 0
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

    def tick(self, time_step: float) -> None:
        self.counter += 1
        self.emit("event")

    def reset(self) -> None:
        self.counter = 0

    def duplicate(self, xs: list[int]) -> list[int]:
        return [x * 2 for x in xs]


# %%
# Nodes need to be added to a network to perform any work
node = SimpleNode(0)
network.add_node(node)

# %%
print(node.description)

# %%
# when the network spins
network.spin(time_step=0.1, duration=1)

# %%
# While the network spins, we could connect to the network
# with a client running in a different process.
# The client may load an Aseba script that uses the defined events, variables and
# functions, like ::
#
#   onevent event
#   emit user_event_1 counter
#
#   onevent user_event_2
#   call duplicate args[1:2] args[2:3]
