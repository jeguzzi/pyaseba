==========
Node shell
==========

Pass a valid Dashel target to 

.. code-block::

   $ python -m pyaseba.client.node --target tcp:port=33333

   Welcome to the pyaseba client node shell. Type help or ? to list commands.

   (node) 

to enter a command shell that exposes most :py:class:`pyaseba.client.Node` methods as commands


.. code-block::

   (node) help
   
   Documented commands (type help <topic>):
   ========================================
   call         events  exposed    get   mirrored  script  variables
   description  exit    functions  help  quit      set     wait  
   
   (node) help description
   Print the description:  description


We can print its description

.. code-block::

   (node) description

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
   - square(input[1]): set value to the square of the input

get variables,

.. code-block::

   (node) get counter
   [4404]  

set variables,

.. code-block::

   (node) set value 123

and more.