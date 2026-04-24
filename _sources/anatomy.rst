Anatomy of a client
===================

Lifecycle
---------

.. tabs::

   .. tab:: Synchronous

      .. code-block:: python

         from pyaseba import Client

         client = Client(query=False)
         ...
         client.close()

   .. tab:: Asynchronous

      .. code-block:: python

         from pyaseba import ClientAsync

         client = ClientAsync(query=False)
         ...
         client.close()      

Connection
----------

.. code-block:: python

   from pyaseba import Client

   connection = client.connect(...)


or with a context manager

.. tabs::

   .. tab:: Synchronous

      .. code-block:: python

         from pyaseba.client import connect
      
         with connect(...) as client:
            ...

   .. tab:: Asynchronous

      .. code-block:: python
      
         from pyaseba.client import connect_async

         async with connect_async(...) as client:
            ...

Multiple connections
--------------------

.. code-block:: python

   client.connect(...)
   client.connect(...)
   client.connections
   ...

Peers (internal connection)
---------------------------

.. code-block:: python
   
   Client(port=...)

example:

.. code-block:: python

   client1 = Client(port=10001)
   client2 = Client(port=10002)
   client2.connect("tcp:host=127.0.0.1;port=10001")
   
   print(client1.connections)
   print(client2.connections)


Scan for nodes
--------------

Get connected nodes without querying their description

.. tabs::

   .. tab:: Synchronous

      .. code-block:: python

         nodes = client.scan(number=..., wait_ms = ...)

   .. tab:: Asynchronous

      .. code-block:: python
      
         nodes = await client.scan(number=...)


Query description
-----------------

.. tabs::

   .. tab:: Synchronous

      .. code-block:: python

         for node in nodes:
            description = client.query(node=node, wait_ms=...)

   .. tab:: Asynchronous

      .. code-block:: python
      
         for node in nodes:
            description = await client._query(node=node)


Query automatically
-------------------

If ``Client(query=True)`` node description are queried automatically.


.. tabs::

   .. tab:: Synchronous

      .. code-block:: python

         node = client.wait_node_connection(wait_ms=...)
         if node is not None:
            description = client.get_description(node)

   .. tab:: Asynchronous

      .. code-block:: python
      
         node = await client.wait_node_connection()
         description = client.get_description(node)

Callbacks
~~~~~~~~~

Permanent

.. code-block:: python

   def node_has_connected(node):
      ...

   client.add_node_connection_callback(node_has_connected)


Once

.. code-block:: python

   def node_has_connected(node):
      ...

   client.wait_node_connection(callback=node_has_connected)

   



