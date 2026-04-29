"""
Nodes attributes
================

Showcases the Python-like :py:class:`pyaseba.client.Node` interface.
that exposes Aseba variables and functions as Python attributes and methods
"""

from pyaseba.client import Node
from pyaseba.client.node import EventSpec, MirroringConfig

# %%
# When specialized, py:class:`pyaseba.client.Node` offer a higher-level,
# more Python-affine interface:
#
# - Python properties for a selection of Aseba variables.
#
# - Python methods ``node.call_<function>(...)`` as an alternative to
#   ``node.call(<function>, ...)``.
#
# In this example, the node exposes properties
# ``counter`` and ``value``, and method `call_square``.


class MySimpleNode(Node):
    mirroring_config = MirroringConfig(
        events={"event": EventSpec(variables=["counter"])},
        function_include=["square"])
    properties = ['counter', 'value']
    functions = ['square']


node = MySimpleNode(cached=True)
node.connect(target="tcp:port=33333", start_mirroring=True)

# %%
# Aseba variables are now accessible as attributes

node.value = 3  # type: ignore[attr-defined]

# %%
# Pushing changes from Python to Aseba is postponed until
# :py:meth:`pyaseba.client.Node.sync` is called.

node.sync()
node.value  # type: ignore[attr-defined]

# %%
# Calling

node.call_square(3)  # type: ignore[attr-defined]

# %%
# should set ``value`` to
# the square of 3. As ``value`` is not synchronized,
# we need to query it explicitly.

import time
time.sleep(1)

print(node.get("value", cached=False))

# %%
node.close(reset=True)
