"""
Automatic node discovery
========================

Showcases how the client automatically discovers nodes
and gets their description.
"""

from pyaseba.client import Client

client = Client()
connection = client.connect("tcp", port=33333)
connection

# %%
# The client regularly broadcasts a discovery messages. Remote node that
# answer are asked to send their description. The following method
# waits until the description are received.

client.wait_nodes(wait_ms=500)

# %%
# Discovered node ids are kept in memory

client.node_ids

# %%
# as well as their description.

client.descriptions

# %%
# Alternatively, if we wait for a single node, we
# can call

node_id, connection = client.wait_node(wait_ms=500)
node_id, connection

# %%
# and

client.get_description(node_id=node_id)

# %%
client.close()
