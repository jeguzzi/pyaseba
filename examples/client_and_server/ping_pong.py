from functools import partial
from pyaseba import Client, Network
from pyaseba.client import Event


def cb(event: Event, client: Client, node_id: int) -> None:
    if event.name == "ping":
        print('pong')
        client.emit_event(node_id=node_id, name="pong")


def make_client() -> Client:
    client = Client()
    if client.connect("tcp:host=127.0.0.1;port=33333", max_retries=1):
        node_id, conn = client.wait_node(wait_ms=1000)
        client.load_script(node_id=node_id,
                           events={"pong": 0},
                           script="""
onevent pong
emit ping
""")
        client.cmd_run(node_id)
        client.add_event_callback(partial(cb, node_id=node_id, client=client))
        client.emit_event(node_id, name="pong")
    return client


def make_network() -> Network:
    from pyaseba.network import Node

    class MyNode(Node):
        functions = {'ping': ('', [])}

    network = Network()
    node = MyNode(0, "MyNode", default_variables=False, default_functions=False)
    network.add_node(node)
    return network


if __name__ == '__main__':
    network = make_network()
    client = make_client()
    network.spin(time_step=0.1, duration=10)
    client.close()
