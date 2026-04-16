import argparse
import asyncio
import sys

from pyaseba import ClientAsync


async def main(target: str) -> None:
    client = ClientAsync()
    if target:
        await client.connect(target, max_retries=0)
    print(f"Connections: {client.connections}")
    connection, new_target = await client.wait_connection(wait_ms=1000)
    if connection:
        print(f'Added connection #{connection} to {new_target}')
        r = client.wait_disconnection(connection, wait_ms=1000)
        if r:
            print("Disconnected")
        else:
            print("Not yet disconnected")
    else:
        print("No new connections")
    client.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:port=33333")
    args = parser.parse_args()
    try:
        asyncio.run(main(args.target))
    except Exception as e:
        sys.exit(f"ERROR: {e}")

