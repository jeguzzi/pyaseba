"""
Nodes mirroring
===============

Showcases how :py:class:`pyaseba.client.Node` mirrors local Aseba events
and expose local Aseba functions to Python.
"""

from pyaseba.client import Node
from pyaseba.client.node import EventSpec
import time

# %%
# When specialized, py:class:`pyaseba.client.Node` may expose
# local Aseba events and functions of the remote node to Python
# by loading and running a custom Aseba script.
#
# In particular, they:
#
# - Define user events that mirror local events. Each time a
#   (remote) local event is emitted, the user events is forwarded
#   to the client, together with a list of variables.
#   This keeps the value of the Aseba variables in sync.
#   Moreover, local events can be awaited using
#   :py:meth:`pyaseba.client.Node.wait`.
#
# - Define user events to call remote native functions.
#   For example, a native function f that takes an input of size 2,
#   is exposed by a user event ``call_f`` that takes a size 2 payload.
#   Users can trigger a remote function call using
#   :py:meth:`pyaseba.client.Node.call`.
#
# In this example, the node mirrors local event (``event``), synchronizing the value
# of variable ``counter`` (which is incremented by the remote node just before emitting a
# the event) and exposes the Aseba function ``square``, which set the Aseba variable ``value``
# to the square of the function argument.


class MySimpleNode(Node):
    events = {"event": EventSpec(variables=["counter"])}
    function_args = "args"


node = MySimpleNode(cached=True)
node.connect(target="tcp:port=33333")

# %%
# The node has loaded the Aseba script, created from
# the class specification.

print(node.script)

# %%
# It exposes local functions

print(node.exposed_functions)

# %%
# which we can call using using

node.call("square", 3)

# %%
# Let us verify that this indeed sets ``value`` to
# the square of 3. As the ``value`` is not synchronized,
# we need to query it explicitly.

node.get("value", cached=False)

# %%
# It also mirrors local events

print(node.mirrored_events)

# %%
# which we can wait for.
# As the node is increasing ``counter`` variable each time, we expect to see it
# reflected in the ``counter`` attribute.

for _ in range(5):
    node.wait('event')
    print(f'counter = {node.get("counter")}')

# %%
# We can also define callbacks for (mirrored) local events, like


def cb(node: MySimpleNode) -> None:
    print(f'counter = {node.get("counter")}')


node.set_callback("event", cb)

# %%
# Sleeping for a while should get callback called several times

time.sleep(1)

# %%
node.close(reset=True)
