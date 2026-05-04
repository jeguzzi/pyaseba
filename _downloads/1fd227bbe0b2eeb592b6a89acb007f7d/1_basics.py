"""
Nodes basics
============

Showcases the basic :py:class:`pyaseba.client.Node` interface.
"""

from pyaseba.client import Node

# %%
# :py:class:`pyaseba.client.Node` are stateful objects linked to a
# specific remote Aseba node connected through a :py:class:`pyaseba.client.Client`.
#

node = Node(cached=False)
node.connect(target="tcp:port=33333")
node

# %%
# While most :py:class:`pyaseba.client.Client` methods require selecting a ``node_id``,
# :py:class:`pyaseba.client.Node` methods keep a reference to the node ID and network,
# which can be used to, for example, set and get variables.

node.set("value", 1)
node.get("value")

# %%
# Nodes keep an optional cache of Aseba variables values.

node.get("value", cached=True)

# %%
node.set("value", 2, cached=True)

# %%
# When caching, modified values are not sent to the remote node.

node.get("value", cached=False)

# %%
# Until calling :py:meth:`pyaseba.client.sync`
node.sync()
node.get("value", cached=False)

# %%
# Closing the node will also close the client.
node.close(reset=True)
