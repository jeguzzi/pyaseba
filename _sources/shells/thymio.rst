============
Thymio shell
============

The Thymio shell is a :doc:`node` pre-configured for the Thymio robot using :py:class:`pyaseba.client.thymio.Thymio`.

Execute

.. code-block::

   $ python -m pyaseba.client.thymio

   Welcome to the pyaseba client node shell.   Type help or ? to list commands.list commands.

   (thymio) 

to enter a command shell that exposes most :py:class:`pyaseba.client.Node` methods as commands


.. code-block::

   (thymio) help
   
   Documented commands (type help <topic>):
   ========================================
   call         events  exposed    get   mirrored  script  variables
   description  exit    functions  help  quit      set     wait  
   
   (thymio) help description
   Print the description:  description


We can print its description

.. code-block::

   (thymio) description

   Node 48069
   ==========
   
   Variables
   ---------
   - _fwversion[2]
   - _id[1]
   - _imot[2]
   - _integrator[2]
   - _productId[1]
   - _vbat[2]
   - acc[3]
   - acc._tap[1]
   ...

get variables,

.. code-block::

   (thymio) get prox.horizontal
   [3830, 4401, 2584, 0, 0, 0, 0]

set variables,

.. code-block::

   (thymio) set leds.top 0 32 0

wait for events,

.. code-block::

   (thymio) wait button.center

call functions

.. code-block::

   (thymio) call sound.system 5

and more.