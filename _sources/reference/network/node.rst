Node
====

.. py:currentmodule:: pyaseba.network

.. autoclass:: Node
   :members:
   :undoc-members:

   .. py:attribute:: events
      :type: dict[str, str]

      A dictionary of ``{name: description}``
      Set it in sub-classes to define local events;
      the base class does not set the attribute. 

   .. py:attribute:: functions
      :type: dict[str, tuple[str, list[tuple[str, int]]]]

      A dictionary of ``{name: (description, arguments}``,
      where each argument is a tuple ``(name, size)``.
      Set it in sub-classes to define variables;
      the base class does not set the attribute. 

   .. py:attribute:: variables
      :type: dict[str, int]

      A dictionary of ``{name: size}``.
      Set it in sub-classes to define variables;
      the base class does not set the attribute. 

      
