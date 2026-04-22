import asyncio
from functools import partial

from pyaseba import ClientAsync, Network
from pyaseba.client import Event, wrap_callback
from pyaseba.network import Node


async def cb(event: Event,
             client: ClientAsync,
             node_id: int,
             sleep: float = 0.1) -> None:
    if event.name == "ping":
        await asyncio.sleep(sleep)
        print('pong')
        client.emit_event(node_id=node_id, name="pong")


async def run_client(script: str, sleep: float) -> None:
    client = ClientAsync(address="localhost")
    if await client.connect("tcp:port=33333;host=localhost", max_retries=1):
        node_id, conn = await client.wait_node(wait_ms=1000)
        if conn:
            client.load_script(node_id=node_id,
                               events={
                                   "pong": 0,
                                   "ping": 0
                               },
                               script=script)
            client.cmd_run(node_id)
            client.add_event_callback(
                wrap_callback(
                    partial(cb, node_id=node_id, client=client, sleep=sleep)))
            print('Sending first pong')
            client.emit_event(node_id, name="pong")
            await client.wait_disconnection(wait_ms=1000)
        else:
            raise RuntimeError("Could not connect to a node")
    else:
        raise RuntimeError("Could not connect to target")
    client.close()


async def run_network(node_cls: type[Node]) -> None:
    network = Network(address="localhost")
    node = node_cls(0, "Node", default_functions=False)
    network.add_node(node)
    return await network.spin_async(time_step=0.1, duration=1)


async def main(node_cls: type[Node], script: str, sleep: float) -> None:
    await asyncio.gather(run_network(node_cls), run_client(script, sleep))
