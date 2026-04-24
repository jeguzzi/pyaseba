"""
Callbacks
=========

Showcases the different callbacks that clients support.
"""

from pyaseba.client import Message, Event, Client
import time

client = Client()

# %%
# Networks connection and disconnection
# -------------------------------------
#
# Callbacks for when new networks are connected
# and/or disconnected.


def connection_callback(connection: int, target: str) -> None:
    print(f"Connected network #{connection} at {target}")


client.add_connection_callback(connection_callback)
client.connect("tcp", port=33333)

# %%
# Nodes
# -----
#
# Callbacks for when new nodes are discovered and/or disconnected.


def node_callback(node_id: int, connection: int) -> None:
    print(f"Discovered node #{node_id} on network #{connection}")


client.add_node_callback(node_callback)
node_id, _ = client.wait_node()


# %%
# Messages
# --------
#
# Callbacks for when messages are received.


def message_callback(message: Message, connection: int) -> None:
    print(f"Received {message} on network #{connection}")


client.add_message_callback(message_callback)
time.sleep(1)
client.clear_message_callbacks()


# %%
# Events
# --------
#
# Callbacks for when events are received.

script = """
onevent event
emit count counter
"""

client.load_script(node_id=node_id,
                   script=script,
                   events={"count": 1})
client.cmd_run(node_id)


def event_callback(event: Event) -> None:
    print(f"Received {event}")


client.add_event_callback(event_callback)
time.sleep(1)
client.clear_message_callbacks()

# %%
client.close()
