# Pyaseba

Pyaseba provides Python bindings for [Aseba](https://github.com/aseba-community/aseba) using [pybind11](https://pybind11.readthedocs.io/en/stable).

Aseba is a light-way communication protocol and scripting language which works from tiny micro-controllers to your fully feature PC, developed at EPFL Lausanne. For the [words](https://github.com/aseba-community/aseba#aseba) of its authors

> Aseba is a set of tools which allow novices to program robots easily and efficiently. For these reasons, Aseba is well-suited for robotic education and research. Aseba is an open-source software created by Dr. Stéphane Magnenat with contributions from the community.

It is known as the scripting language of the educational [Thymio](https://www.thymio.org) robot. Programming it is normally done in a GUI client, like Aseba Studio or the Thymio VPL.

Pyaseba offers an alternative to GUI clients to interact with any robot that runs Aseba (or more generally with any Aseba network). With Pyaseba, you can perform common operations like reading and writing Aseba variables or loading Aseba scripts but also more low-level actions, like sending and receiving Aseba messages directly.

Pyaseba provides two interfaces: 1) a lower-level client API that let you interact with a collection of Aseba nodes and 2) a higher-level client node API to interact with a single Aseba node, exposing to Python its native variables, events and functions. 
The clients can connect to multiple Aseba networks at once, each comprising potentially multiple Aseba nodes.

Moreover, it comes with its own local Aseba network that runs Aseba nodes implemented in Python, i.e., nodes with Python native functions and Python control steps. Its primary motivation is to test the client interfaces but it may be used to extend an Aseba network with custom nodes, for example for teaching.

## Example

Assuming a Thymio is connected to one of the serial ports, we can use pyaseba to interact with it from Python

```python
>>> from pyaseba import Client
>>> client = Client()
>>> client.connect("ser:name=Thymio")
1
>>> node, _ = client.wait_node()
```

to read Aseba variables, like the proximity sensor readings,

```python
>>> client.get_variable(node, "prox.horizontal")
[3860, 4339, 2546, 0, 0, 0, 0]
```

to set Aseba variables, e.g. to change LED colors

```python
>>> client.set_variable(node, "leds.top", [0, 32, 0])
```

to load and run Aseba scripts, e.g. to play a sound

```python
>>> client.load_script(node, "call sound.system(1)")
>>> client.cmd_run(node)
```

Pyaseba also exposes the Aseba low-level messaging protocol, for instance to send

```python
from pyaseba.client.msgs import ListNodes
>>> client.send_message(ListNodes())
```

and receive message 

```python
>>> client.get_message()
(NodePresent(source=48069, version=9), 1)
```

The repository and the documentation contains an extensive collection of examples.

## Documentation

The documentation is [published online](https://jeguzzi.github.io/pyaseba).


## License and Dependencies

Pyaseba is distributed under [MIT license](https://github.com/jeguzzi/pyaseba/blob/main/LICENSE). It make use of the open source 
dependencies listed below without redistributing them.

### Aseba

Pyaseba can be build using the version of Aseba maintained by the [Aseba-community](https://github.com/aseba-community/aseba) and distributed under LGPL-3.0 license or the version maintained by the [Mobsya association](https://github.com/Mobsya/aseba), also distributed under LGPL-3.0 license.

The Mobsya version include additional message to interact with the latest version of the Thymio robot. 

Pyaseba is using Aseba source code without redistributing it.

### Dashel

Pyaseba uses the source code of [Dashel](https://github.com/aseba-community/dashel), distributed under BSD-3-Clause license, without redistributing it.

### Pybind11

Pyaseba is using Aseba source code without redistributing it.

### Spdlog, Fmt and pybind11_log

If pyaseba is build with logging support, it uses [Spdlog](https://github.com/gabime/spdlog), [fmt](https://github.com/fmtlib/fmt), and [pybind11_log](https://github.com/tanghaibao/pybind11_log) without redistributing them.






