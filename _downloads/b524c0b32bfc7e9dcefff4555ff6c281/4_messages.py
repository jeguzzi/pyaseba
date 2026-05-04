"""
Messages
========

Illustrate how a client receives an Aseba message.
"""

from pyaseba.client import Client
from pyaseba.client.msgs import GetVariables, Variables

client = Client()
connection = client.connect("tcp", port=33333)

# %%
# Receiving messages blocks until the next message is received.

msg, connection = client.get_message()
print(f"Got message {msg} from network #{connection}")

# %%
# Sending messages is (almost) instantaneous instead.
# Let us send a (manual) request for some variables
# and wait until we get a response.

client.send_message(GetVariables(dest=0, start=0, length=10))
for _ in range(5):
    msg, connection = client.get_message()
    if isinstance(msg, Variables):
        print(f"Got response {msg}")
        break
    else:
        print(f"Ignoring {msg}")


client.close()
