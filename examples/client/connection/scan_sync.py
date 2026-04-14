import argparse

from pyaseba import Client


def main(target: str, number: int) -> None:
    client = Client(ping_period_ms=0, automatic_query=False)
    if client.connect(target, max_retries=1):
        nodes = client.scan(wait_ms=1000, number=number)
        targets = client.connections
        for t, ns in nodes.items():
            print(f'Found nodes {ns} on {targets.get(t, "?")}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:host=127.0.0.1;port=33333")
    parser.add_argument('--number', default=-1, type=int)
    args = parser.parse_args()
    main(args.target, args.number)
