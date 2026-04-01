import asyncio


async def run_client() -> None:
    from pyaseba import ClientAsync
    from pyaseba.client import Event, Message

    def cb(event: Event) -> None:
        if event.name == "ping":
            print('pong')
            client.emit_event(event.source, "pong")

    def msg_cb(msg: Message) -> None:
        print(msg)

    def conn_cb(node: int) -> None:
        print(f"Connected {node}")

    client = ClientAsync()
    # client.add_message_callback(msg_cb)
    # client.add_connection_callback(conn_cb)
    print('wait connection')
    if await client.connect("tcp:host=127.0.0.1;port=33333", max_retries=1):
        print('connected ... wait node')
        node = await client.wait_node_connection()
        assert node is not None
        print('load script')
        client.load_script(node=node,
                           events=[("pong", 0), ("ping", 0)],
                           script="""
onevent pong
call ping()
emit ping
""")
        client.run(node=node)
        client.add_event_callback(cb)
        print('send first pong')
        client.emit_event(node, "pong")
        await asyncio.sleep(2)
    client.close()


async def run_server() -> None:
    from pyaseba.server import Node, Server

    class MyNode(Node):
        events: list[str] = []
        variables: list[tuple[str, int]] = []
        functions: list[tuple[str, list[tuple[str, int]]]] = [('ping', [])]

        def ping(self) -> None:
            print('ping')

    server = Server()
    node = MyNode(0, "MyNode")
    server.add_node(node)
    print('Start node loop')
    for _ in range(20):
        server.spin(0.1)
        await asyncio.sleep(0.1)
    print('End node loop')


async def main() -> None:
    s = asyncio.create_task(run_server())
    c = asyncio.create_task(run_client())
    await s
    await c


if __name__ == '__main__':

    asyncio.run(main())
