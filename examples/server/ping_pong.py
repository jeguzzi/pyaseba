import time
from functools import partial
from pyaseba import Client, Server
from pyaseba.client import Event


def cb(event: Event, client: Client, node: int) -> None:
    if event.name == "ping":
        print('pong')
        client.emit_event(node=node, name="pong")


def client() -> Client:
    client = Client()
    if client.connect("tcp:host=127.0.0.1;port=33333", max_retries=1):
        node = client.wait_node_connection(wait_ms=100)
        assert node
        client.load_script(node=node,
                           events=[("pong", 0)],
                           script="""
onevent pong
emit ping
""")
        client.run(node=node)
        client.add_event_callback(partial(cb, node=node, client=client))
        client.emit_event(node=node, name="pong")
    return client


def server() -> Server:
    from pyaseba.server import Node

    class MyNode(Node):
        events: list[str] = []
        variables: list[tuple[str, int]] = []
        functions: list[tuple[str, list[tuple[str, int]]]] = [('ping', [])]

    server = Server()
    node = MyNode(0, "MyNode")
    server.add_node(node)
    return server


if __name__ == '__main__':
    sn = server()
    cn = client()
    for _ in range(100):
        sn.spin(0.1)
        time.sleep(0.1)
    cn.close()
