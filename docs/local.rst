===================
Local Aseba network
===================

If you do not have a Thymio but want to try out ``pyaseba``, run your own Aseba network

.. code-block::

   $ python -m pyaseba.examples.network.simple --number 1

before interacting with it in a separate Python console by connecting to ``tcp:port=33333``.

Client
======

We follow similar steps as with the Thymio. First, we connect and discover nodes:

.. code-block::

   $ python

   >>> from pyaseba import Client
   >>> client = Client()
   >>> client.connect("tcp:port=33333")
   1
   >>> node_id, conn = client.wait_node()
   >>> node_id, conn
   (0, 1)

A positive ``node_id`` means that we discovered the Aseba node 
running on the local Aseba network. We can use it to get/set variables

.. code-block::

   >>> list(client.get_description(node_id).variables)
   ['_productId', 'args', 'counter', 'id', 'source', 'value']
   >>> client.get_variable(node_id, 'value')
   [0]
   >>> client.set_variable(node_id, "value", [32])

Unfortunately, contrary to a robot, the local Aseba node is not doing much except manipulating values. Therefore, to check that the interaction is working, we just get the value again 

.. code-block::

   >>> client.get_variable(node_id, "value")
   [32]

Let us inspect the full description of the node

.. code-block::

   >>> from pyaseba import print_description
   >>> print_description(node_id, client.get_description(node_id))
   Node 0
   ======
   
   Variables
   ---------
   - _productId[1]
   - args[32]
   - counter[1]
   - id[1]
   - source[1]
   - value[1]
   
   Local events
   ------------
   - event: emitted at each control step after incrementing counter
   
   Functions
   ---------
   - duplicate(input[1], result[1]): duplicates the input
   - square(input[1]): sets value to the square of the input

Let us verify that counter is indeed incremented

.. code-block::

   >>> client.get_variable(node_id, 'counter')
   [4819]
   >>> client.get_variable(node_id, 'counter')
   [4827]

We can load an Aseba script that sends down ``counter`` each time it is updated, and ask the node to run it

.. code-block::

   >>> script = """
   onevent event
   emit counter_incremented counter

   onevent reset
   counter = 0
   """
   >>> client.load_script(node_id, script, events={'counter_incremented': 1, 'reset': 0})
   >>> client.cmd_run(node_id)
  
We can now send an event from Python, which in this case, should make the counter reset

.. code-block::

   >>> client.emit_event(node_id, "reset")
   >>> client.get_variable(node_id, "counter")
   [16]

The client listens for incoming events. For example, let us wait
until the counter is incremented

.. code-block::

   >>> for _ in range(3):
   ...    print(client.get_event(node_id, "counter_incremented"))
   ...    
   Event(source=0, name='counter_incremented', data=[507])
   Event(source=0, name='counter_incremented', data=[508])
   Event(source=0, name='counter_incremented', data=[509])


Node
====

The first step is to configure the Python interface by listing
which events we want to receive (and which variables they are modified) and which variables we want to expose as attributes (properties). This requires knowing the specifics of the remote node we want to interact with.

.. code-block::

   $ python

   >>> from pyaseba.client.node import Node, EventSpec, MirroringConfig

   >>> class MyNode(Node):
   ...    events = MirroringConfig(events={'event': EventSpec(variables=['counter'])})
   ...    properties = ['value', 'counter']
   ...    functions = ['square']
   ...    default_target = "tcp:port=33333"
   ...


The next step is to connect to the node:

   >>> node = MyNode()
   >>> node.connect(start_mirroring)
   True


We can get and set variables by name 

.. code-block::

   >>> list(node.description.variables)
   ['_productId', 'args', 'counter', 'id', 'source', 'temp', 'value']
   >>> node.get("value")
   0
   >>> node.set("value", 1)

or using Python attributes

.. code-block::

   >>> node.value
   1
   >>> node.value = 2
   >>> node.sync()

.. note::

   The ``node`` object caches variable values.
   The :py:meth:`pyaseba.client.Node.sync` forwards to the robot
   any variable we have changed locally. When calling 
   :py:meth:`pyaseba.client.Node.get` and :py:meth:`pyaseba.client.Node.set`, we can specify if we want to query/forward variables or if we prefer to get/set them from the cache. Instead, attributes like ``node.value`` always use the cache.


We can call local functions by name, for example to set ``value``  to the square of the argument,

.. code-block::

   >>> node.call("square", 7)
   >>> node.get("value")
   49

or through specific methods

.. code-block::

   >>> node.call_square(11)
   >>> node.get("value")
   121

We can wait for local events

.. code-block::

   >>> for _ in range(3):
   ...     _ = node.wait("event")
   ...     print(node.counter)
   343
   344
   345

The following Python callback resets the counter every two steps

.. code-block::

   >>> def cb(node):
   ...     if node.counter % 2 == 0:
   ...         node.counter = 0
   ...     node.sync()

It implements in Python the same logic of this Aseba script

.. code-block::

   onevent event
   if counter % 2 == 0 then
     counter = 0
   end

when assigned to the local event by name

   >>> node.set_callback("event", cb)

or using an attribute

.. code-block::

   >>> node.on_event = cb

Let us verify that it works as expected

.. code-block::

   >>> for _ in range(5):
   ...     _ = node.wait("event")
   ...     print(node.counter)
   ...
   1
   0
   1
   0
   1
