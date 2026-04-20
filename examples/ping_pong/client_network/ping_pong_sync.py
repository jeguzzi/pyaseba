import sys
import time
from functools import partial

from pyaseba import Client, Network
from pyaseba.client import Event
from pyaseba.network import Node


def cb(event: Event, client: Client, node_id: int, sleep: float = 0.1) -> None:
    if event.name == "ping":
        time.sleep(sleep)
        print('pong')
        client.emit_event(node_id=node_id, name="pong")


def run_client(script: str, sleep: float) -> None:
    client = Client()
    if client.connect("tcp:port=33333", max_retries=0):
        node_id, conn = client.wait_node(wait_ms=1000)
        if conn:
            client.load_script(node_id=node_id,
                               events={"pong": 0, "ping": 0},
                               script=script)
            client.cmd_run(node_id)
            client.add_event_callback(partial(cb, node_id=node_id, client=client, sleep=sleep))
            print('Sending first pong')
            client.emit_event(node_id, name="pong")
            client.wait_disconnection(conn)
        else:
            raise RuntimeError("Could not connect to a node")
    else:
        raise RuntimeError("Could not connect to target")
    client.close()


def make_network(node_cls: type[Node] = Node) -> Network:
    network = Network()
    node = node_cls(0, "Node", default_functions=False)
    network.add_node(node)
    return network


def main(node_cls: type[Node], script: str, sleep: float) -> None:
    network = make_network(node_cls)
    network.start(time_step=0.1, duration=-1)
    run_client(script, sleep)
    network.stop()
