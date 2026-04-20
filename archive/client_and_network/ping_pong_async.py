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
        node_id, conn = await client.wait_node()
        assert conn
        print('load script')
        client.load_script(node_id=node_id,
                           events={"pong": 0, "ping": 0},
                           script="""
onevent pong
call ping()
emit ping
""")
        client.cmd_run(node_id)
        client.add_event_callback(cb)
        print('send first pong')
        client.emit_event(node_id, "pong")
        await asyncio.sleep(2)
    client.close()


async def run_server() -> None:
    from pyaseba.network import Node, Network

    class MyNode(Node):
        functions = {'ping': ('', [])}

        def ping(self) -> None:
            print('ping')

    network = Network()
    node = MyNode(0, "MyNode", default_variables=False, default_functions=False)
    network.add_node(node)
    print('Start node loop')
    for _ in range(20):
        # TODO: make it async
        network.spin(time_step=0.1, duration=0.1)
        await asyncio.sleep(0.1)
    print('End node loop')


async def main() -> None:
    s = asyncio.create_task(run_server())
    c = asyncio.create_task(run_client())
    await s
    await c


if __name__ == '__main__':

    asyncio.run(main())
