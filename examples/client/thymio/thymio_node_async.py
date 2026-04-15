import argparse
import asyncio
import typing
from collections.abc import Callable

from pyaseba.client import NodeAsync
from pyaseba.client.thymio import ThymioAsync as Thymio
from typing import Awaitable


def control(
        done: asyncio.Future[None]) -> Callable[[NodeAsync], Awaitable[None]]:

    async def f(node: NodeAsync) -> None:
        values = typing.cast('list[int]', await node.get("prox.ground.delta",
                                                         cached=True))
        if any(v < 100 for v in values):
            node.set("motor.left.target", 0)
            node.set("motor.right.target", 0)
            node.set("leds.top", [32, 0, 0])
            await asyncio.sleep(0.5)
            if not done.done():
                done.set_result(None)

    return f


def switch() -> Callable[[NodeAsync], Awaitable[None]]:
    moving = False

    async def f(node: NodeAsync) -> None:
        nonlocal moving
        if not await node.get("button.forward", cached=True):
            if moving:
                node.set("motor.left.target", 0)
                node.set("motor.right.target", 0)
                node.set("leds.top", [0, 0, 0])
                node.call("leds.buttons", 0, 0, 0, 0)
            else:
                node.set("motor.left.target", 100)
                node.set("motor.right.target", 100)
                node.set("leds.top", [32, 32, 0])
                node.call("leds.buttons", 32, 0, 0, 0)
            moving = not moving

    return f


async def main(target: str) -> None:
    node: NodeAsync = Thymio()
    done: asyncio.Future[None] = asyncio.Future()
    node.set_callback("prox", control(done))
    node.set_callback("button.forward", switch())
    if await node.connect(target=target):
        await done
        node.call("leds.buttons", 0, 0, 0, 0)
    else:
        print(f'Could not find a Thymio on {target}')
    await node.close(reset=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:host=127.0.0.1;port=33333")
    args = parser.parse_args()
    asyncio.run(main(args.target))
