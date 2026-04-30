============
Introduction
============

Aseba is a lightweight communication protocol and scripting language which works from tiny microcontrollers to your fully featured PC, developed at EPFL Lausanne. For the `words <https://github.com/aseba-community/aseba#aseba>`_ of its authors:

   Aseba is a set of tools which allow novices to program robots easily and efficiently. For these reasons, Aseba is well-suited for robotic education and research. Aseba is an open-source software created by Dr. Stéphane Magnenat with contributions from the community.

It is known as the scripting language of the educational `Thymio <https://www.thymio.org>`_ robot. Programming it is normally done in a GUI client, like Aseba Studio or the Thymio VPL.

Pyaseba offers an alternative to GUI clients to interact with any robot that runs Aseba (or more generally with any Aseba network). With Pyaseba, you can perform common operations like reading and writing Aseba variables or loading Aseba scripts but also more low-level actions, like sending and receiving Aseba messages directly.

Pyaseba provides two interfaces: 1) a lower-level client API that lets you interact with a collection of Aseba nodes and 2) a higher-level client node API to interact with a single Aseba node, exposing to Python its native variables, events, and functions. 
The clients can connect to multiple Aseba networks at once, each comprising potentially multiple Aseba nodes.

Moreover, it comes with its own local Aseba network that runs Aseba nodes implemented in Python, i.e., nodes with Python native functions and Python control steps. Its primary motivation is to test the client interfaces, but it may be used to extend an Aseba network with custom nodes, for example, for teaching.


Installation
============

Install from PyPi

.. code-block::

   pip install pyaseba

or build from source

.. code-block::

   pip install https://github.com/jeguzzi/pyaseba.git

Next
====

Try the :doc:`first_steps`, have a look at the runnable :doc:`gallery/index` or at the more extensive collection of examples in the `repository <https://github.com/jeguzzi/pyaseba/tree/main/examples>`_, or check out the :doc:`reference/index`.