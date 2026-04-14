"""
Events
======

Shows how a client load an Aseba script to a remote node,
and interacts with it using user-defined events.
"""

from pyaseba.client import Client

client = Client()
client.connect("tcp", port=33333)
node_id, connection = client.wait_node()

# %%
# The remote node defines variables

desc = client.get_description(node_id=node_id)
assert desc
desc.variables

# %%
# and events

desc.local_events

# %%
# which are accessible in Aseba scripts, like this one

script = """
onevent reset
counter = 0

onevent event
emit count counter
"""

# %%
# where
#
# - event ``"reset"`` resets the counter variable
#   when received on the remote node,
# - event ``"count"`` is broadcasted each time the local
#   event (which is not accessible outside the remote node)
#   is emitted by the remote node.

client.load_script(node_id=node_id,
                   script=script,
                   events={"count": 1, "reset": 0})

# %%
# The description should now contain the new events

desc = client.get_description(node_id=node_id)
assert desc
desc.user_events

# %%
# Let us ask the remote node to start running the script

client.cmd_run(node_id)

# %%
# Let us reset the counter

client.emit_event(node_id=node_id, name="reset")

# and start waiting for ``"count"`` events.

for _ in range(5):
    event = client.get_event(node_id=node_id, name="count", wait_ms=1000)
    print(f"Received {event}")

# %%
client.close()
