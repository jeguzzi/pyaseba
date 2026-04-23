import argparse
import asyncio
import logging
from functools import partial

from pyaseba import ClientAsync, Network
from pyaseba.client import Event, wrap_callback
from pyaseba.examples.utils import setup_logging
from pyaseba.network import Node


async def cb(event: Event,
             client: ClientAsync,
             node_id: int,
             sleep: float = 0.1) -> None:
    if event.name == "ping":
        await asyncio.sleep(sleep)
        logging.info('pong')
        client.emit_event(node_id=node_id, name="pong")


async def run_client(script: str, sleep: float) -> None:
    with ClientAsync(address="localhost") as client:
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
                logging.info('Sending first pong')
                client.emit_event(node_id, name="pong")
                await client.wait_disconnection(wait_ms=1000)
            else:
                raise RuntimeError("Could not connect to a node")
        else:
            raise RuntimeError("Could not connect to target")


async def run_network(node_cls: type[Node]) -> None:
    network = Network(address="localhost")
    node = node_cls(0, "Node", default_functions=False)
    network.add_node(node)
    return await network.spin_async(time_step=0.1, duration=1)


async def main(node_cls: type[Node], script: str, sleep: float) -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('--log_level', default="INFO")
    args = parser.parse_args()
    setup_logging(args.log_level)
    await asyncio.gather(run_network(node_cls), run_client(script, sleep))
