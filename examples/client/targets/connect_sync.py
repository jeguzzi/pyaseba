import argparse
from pyaseba import Client
import sys


def main(target: str) -> None:
    client = Client(port=33334)
    if target:
        client.connect(target, max_retries=0)
    print(f"Connections: {client.connections}")
    connection, new_target = client.wait_connection(wait_ms=1000)
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
        main(args.target)
    except Exception as e:
        sys.exit(f"ERROR: {e}")
