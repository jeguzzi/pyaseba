======
Thymio
======

If you have a Thymio, you can interact with it using ``pyaseba``.

Client
======

The lower-level interface is offered by :py:class:`pyaseba.Client` and its asynchronous counterpart :py:class:`pyaseba.ClientAsync`.

The first step is to connect the serial port and wait until we discover the robot. 

.. code-block::

   $ python

   >>> from pyaseba import Client
   >>> client = Client()
   >>> client.connect("ser:name=Thymio")
   1
   >>> node_id, conn = client.wait_node(wait_ms=5000)
   >>> node_id, conn
   (48069, 1)

A positive ``node_id`` means that we discovered the Aseba node 
running on the robot. We can use it to get/set variables

.. code-block::

   >>> list(client.get_description(node_id).variables)
   ['_fwversion', '_id', '_imot', ...
   >>> client.get_variable(node_id, "temperature")
   [250]
   >>> client.set_variable(node_id, "leds.top", [32, 0, 0])

The robot should have turned red.

We can load an Aseba script and ask the robot to run it

.. code-block::

   >>> script = """
   ... onevent button.center
   ... leds.top = [0, 32, 0]
   ... if button.center == 1 then
   ...   emit pressed
   ... end
   ... 
   ... onevent reset
   ... leds.top = [0, 0, 0]
   ... """
   >>> client.load_script(node_id, script, events={'reset': 0, 'pressed': 0})
   >>> client.cmd_run(node_id)
  
The robot should turn green when you press the central button.

We can now send an event from Python, which in this case, should make the robot switch off the LED,

.. code-block::

   >>> client.emit_event(node_id, "reset")

wait until an event is emitted (press the button after executing the code)

.. code-block::
   
   >>> client.get_event(node_id, "pressed", wait_ms=10000)
   Event(source=48069, name='pressed', data=[])

or trigger a callback when this happen

.. code-block::

   >>> def cb(event):
   ...     print(event.name)
   ...     
   >>> client.add_event_callback(cb)

Try pressing the button again and you should see the ``pressed`` logged on your console.


Node
====

The higher-level :py:class:`pyaseba.client.Node` offers a simplified interface to interact with a single Aseba node, in this case running on the robot.

We go through similar steps as with the client: first, connect the robot:

.. code-block::

   $ python

   >>> from pyaseba.client.thymio import Thymio
   >>> thymio = Thymio()
   >>> thymio.connect()
   True

We can get and set variables by name 

.. code-block::

   >>> list(thymio.description.variables)
   ['_fwversion', '_id', '_imot', ...
   >>> thymio.get("temperature")
   284
   >>> thymio.set("leds.top", [32, 0, 0])

or using Python attributes

.. code-block::

   >>> thymio.temperature
   284
   >>> thymio.leds_top = [0, 0, 32]
   >>> thymio.sync()

The robot should have turned first red and then blue.

.. note::

   The ``thymio`` object caches variable values.
   The :py:meth:`pyaseba.client.Node.sync` forwards to the robot
   any variable we have changed locally. When calling 
   :py:meth:`pyaseba.client.Node.get` and :py:meth:`pyaseba.client.Node.set`, we can specify if we want to query/forward variables or if we prefer to get/set them from the cache. Instead, attributes like ``thymio.leds_top`` always use the cache.

To set other LEDs, we can call local functions by name, like

.. code-block::

   >>> thymio.call("leds.circle", 0, 32, 0, 32, 0, 32, 0, 32)

or, using specific methods

.. code-block::

   >>> thymio.call_leds_circle(0, 32, 0, 32, 0, 32, 0, 32)

We can wait for local events, like a button press. Try pressing the button after running the line below

   >>> thymio.wait("button.center")

The following Python callback implements the same logic as the Aseba script of the previous example.

.. code-block::

   >>> def cb(thymio):
   ...     if thymio.button_center:
   ...         print('pressed')
   ...     thymio.call_leds_top(0, 32, 0)

We can assign it to the local event by name

   >>> thymio.set_callback("button.center", cb)

or using specific methods

.. code-block::

   >>> thymio.on_button_center = cb
  
As before, the robot should turn green when you press the central button and you should get a ``pressed`` printed on your console.
