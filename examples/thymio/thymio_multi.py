import argparse
import asyncio

from pyaseba import ClientAsync
from pyaseba.client.thymio import ThymioAsync


async def main(target: str) -> None:
    thymios: list[ThymioAsync] = []
    client = ClientAsync()

    if await client.connect(target=target):
        await client.wait_nodes(number=2)
    for conn, node_ids in client.node_ids.items():
        for node_id in node_ids:
            thymio = ThymioAsync(record_prox_comm=True)
            if await thymio.connect(client=client, node_id=node_id):
                thymios.append(thymio)

    for i in range(100):
        print(f'step {i}')
        await asyncio.gather(*[thymio.wait('prox') for thymio in thymios])

    for thymio in thymios:
        await thymio.close(reset=True)
    client.close()

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:port=33333")
    args = parser.parse_args()
    asyncio.run(main(args.target))
