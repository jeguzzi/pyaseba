"""
Nodes attributes
================

Showcases the higher-level :py:class:`pyaseba.client.Node` interface.
"""

from pyaseba.client import Node
from pyaseba.client.node import EventSpec
import time

# %%
# When specialized, py:class:`pyaseba.client.Node` offer a higher-level,
# more Python-affine interface by loading and running a custom Aseba script.
#
# In particular, through the Aseba script, they:
#
# 1. Define user events that mirror local events. Each time a
#    (remote) local event is emitted, the user events is forwarded
#    to the client, together with a list of variables.
#    This keeps the value of the Aseba variables in sync.
# 2. Define Python properties for Aseba variables.
#    Forwarding changes from Python to Aseba is postponed until
#    :py:meth:`pyaseba.client.Node.sync` is called.
# 3. Define user events to call remote native functions.
#    For example, a native function f that takes an input of size 2,
#    is exposed by an event "call_f" that takes a size 2 payload.
#
# In this example, the node exposes 1 event, 1 function and 2 values as properties.


class MySimpleNode(Node):
    events = {"event": EventSpec(variables=["counter"])}
    function_prefixes = ('',)
    target = "tcp:port=33333"
    properties = ['counter', 'value']
    functions = ['square']


node = MySimpleNode(cached=True)
node.connect()

# %%
# The node has loaded the Aseba script, created from
# the class specification.

print(node.script)

# %%
# Aseba variables are now accessible as attributes

node.value = 3  # type: ignore[attr-defined]
node.sync()
node.value  # type: ignore[attr-defined]

# %%
# we can call native functions
node.call("square", 3)
# %%
# Let us verify that this as indeed set ``value`` to
# the square of 3. As the ``value`` is not synchronized,
# we need to query it explicitly.

node.get("value", cached=False)

# %%
# Functions can be called with an alternative syntax

node.call_square(4)  # type: ignore[attr-defined]
node.get("value", cached=False)

# %%
# We can wait until a new (mirrored) local event is emitted
# As the node is increasing ``counter`` variable each time, we expect to see it
# reflected in the ``counter`` attribute.
for _ in range(5):
    node.wait('event')
    print(f'counter = {node.counter}')  # type: ignore[attr-defined]

# %%
# We can define a callback for (mirrored) local events, like


def cb(node: MySimpleNode) -> None:
    print(f'Event: counter = {node.counter}')


node.set_callback("event", cb)

# %%
# Sleeping for a while should get callback called several times

time.sleep(1)

# %%
node.close(reset=True)
