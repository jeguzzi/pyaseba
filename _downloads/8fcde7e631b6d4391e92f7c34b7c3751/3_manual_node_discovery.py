"""
Manual node discovery
=====================

This script showcases how the client can discover nodes
and query their description manually.
"""

from pyaseba.client import Client

# %%
# We set up the client to not discover nodes automatically

client = Client(automatic_query=False, ping_period_ms=0)
connection = client.connect("tcp", port=33333)
connection

# %%
# With this configuration, waiting for nodes will not discover them

client.wait_nodes(wait_ms=500)

# %%
# Instead, we need to first manually scan for node IDs

scan = client.scan(wait_ms=500)
scan

# %%
# and then query them for a description

for target, node_ids in scan.items():
    for node_id in node_ids:
        desc = client.query_description(node_id=node_id, wait_ms=500)
        print(f"Node {node_id}: {desc}")

# %%
# Once we have received them,
# IDs and descriptions are accessible like
# for automatically discovered nodes.

client.descriptions

# %%
client.close()
