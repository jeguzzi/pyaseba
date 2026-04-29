"""
Client
======

Showcases the life-cycle of an Aseba client.
"""

# %%
# 1. Create the client
# --------------------

from pyaseba.client import Client

client = Client()

# %%
# 2. Connect one or more Aseba networks
# -------------------------------------

connection = client.connect("udp:")
connection

# %%
# Connections positive integers that index connected
# Dashel targets.

client.connections

# %%
# 3. Interact with the network[s]
# -------------------------------
# See the other examples.
#
# 4. Clean up
# -----------
#
# Once done, either close the connections one by one::
#
#   client.close_connection(connection)
#
# or close the client directly, which in turn closes all connections.

client.close()
client.connections

# %%
#
# .. hint::
#
#    The client will automatically shutdown when it is deleted.
#    Therefore cleaning up is not strictly needed unless you want to release
#    resources before the client is deleted,
#    which would happen latest when the script exits.

