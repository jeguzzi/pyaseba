import argparse
import asyncio

from pyaseba import ClientAsync
from pyaseba.client.thymio import ThymioAsync


def group_control(thymios: list[ThymioAsync]) -> None:
    ns = set(len(thymio.prox_comm_buffer) for thymio in thymios)
    if len(ns) == 1:
        n = ns.pop()
        for thymio in thymios:
            thymio.leds_top = [0, 0, 32] if n else [0, 0, 0]
            thymio.sync()
        return
    for thymio in thymios:
        thymio.leds_top = [0, 32, 0] if len(
            thymio.prox_comm_buffer) else [32, 0, 0]
        thymio.sync()


async def main(target: str) -> None:
    thymios: list[ThymioAsync] = []
    client = ClientAsync()

    if await client.connect(target=target):
        while len(client.nodes) < 2:
            await asyncio.sleep(1)
    print(client.nodes)
    for i, node in enumerate(client.nodes):
        thymio = ThymioAsync(record_prox_comm=True)
        if await thymio.connect(client=client, node_id=node):
            thymios.append(thymio)
            await thymio.wait('prox')
            thymio.prox_comm_tx = int(node & (2**11 - 1))
            thymio.motor_left_target = -50 * (2 * i - 1)
            thymio.motor_right_target = 50 * (2 * i - 1)
            thymio.sync()
            thymio.call_prox_comm_enable(1)

    try:
        while True:
            await asyncio.gather(*[thymio.wait('prox') for thymio in thymios])
            client.clear_incoming_messages()
            group_control(thymios)
    except:
        pass

    for thymio in thymios:
        await thymio.close(reset=True)
    client.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:host=127.0.0.1;port=33333")
    args = parser.parse_args()
    asyncio.run(main(args.target))
