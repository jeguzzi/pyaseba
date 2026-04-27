============
Client shell
============

Execute 

.. code-block::

   $ python -m pyaseba.client

   Welcome to the pyaseba client shell. Type help or ? to list commands.

   (client) 

to enter a command shell that exposes most :py:class:`pyaseba.client.Client` methods as commands


.. code-block::

   (client) help
   
   Documented commands (type help <topic>):
   ========================================
   connect      events     get_all_variables  help      set_variable
   connections  exit       get_message        node_ids  variables   
   description  functions  get_variable       quit      wait_nodes  
   
   (client) help connect
   Connect to a target:  connect [TARGET]

After we connect a Dashel target

.. code-block::

   (client) connect tcp:port=33333
   1

we can print its description

.. code-block::

   (client) description 0

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

   (client) get_variable 0 counter
   [4404]  

set variables,

.. code-block::

   (client) set_variable 0 value 123

and more.