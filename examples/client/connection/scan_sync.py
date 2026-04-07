import argparse

from pyaseba.client import connect


def main(target: str, number: int) -> None:
    with connect(target, max_retries=1, ping=False) as client:
        nodes = client.scan(wait_ms=1000, number=number)
        print(f'Found nodes {nodes} on {target}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:host=127.0.0.1;port=33333")
    parser.add_argument('--number', default=-1, type=int)
    args = parser.parse_args()
    main(args.target, args.number)
