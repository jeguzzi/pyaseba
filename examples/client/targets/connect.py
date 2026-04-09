import argparse
from pyaseba import Client


def main(target: str) -> None:
    client = Client(port=33333)
    print(client.connect(target, max_retries=0))
    print('close')
    client.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:host=127.0.0.1;port=33333")
    args = parser.parse_args()
    main(args.target)
