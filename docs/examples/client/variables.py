"""
Variables
=========

Shows how to get and set variables
of a (remote) Aseba node.
"""

from pyaseba.client import Client
import time

client = Client()
client.connect("tcp", port=33333)
node_id, connection = client.wait_node()
node_id, connection

# %%
# Let us inspect which variables are defined by the node

desc = client.get_description(node_id=node_id)
assert desc
list(desc.variables)

# %%
# We get the current value of all variables one by one

# TODO: should set the variables to zero
client.cmd_reset(node_id)

for name in desc.variables:
    value = client.get_variable(node_id=node_id, name=name)
    print(f"{name} = {value}")


# %%
# We reset the value to an uniform 1.
for name, (index, size) in desc.variables.items():
    client.set_variable(node_id=node_id, name=name, value=[1] * size)

# %%
# As the node will need a bit of time to set the variable, we
# wait patiently
time.sleep(1)

# %%
# before checking the current values, getting them all at once.
for name, value in client.get_all_variables(node_id=node_id).items():
    print(f"{name} = {value}")

# %%
# .. note::
#
#    Not all values are 1 because in the meantime the node as changed some of them.

# %%
client.close()
