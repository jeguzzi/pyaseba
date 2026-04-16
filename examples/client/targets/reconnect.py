import argparse
import sys

from pyaseba import Client


def main(target: str) -> None:
    client = Client()
    for _ in range(3):
        print("Connecting")
        connection = client.connect(target, max_retries=1)
        print(f"Connections: {client.connections}")
        if connection:
            print(f"Closing connection #{connection}")
            r = client.close_connection(connection)
            if not r:
                raise RuntimeError("Failed closing connection")
            print(f"Connections: {client.connections}")
        else:
            raise RuntimeError("Could not connect")
    client.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:port=33333")
    args = parser.parse_args()
    try:
        main(args.target)
    except Exception as e:
        sys.exit(f"ERROR: {e}")
