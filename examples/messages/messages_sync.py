import argparse

from pyaseba import Network


def main(target: str) -> None:
    network = Network()
    if network.connect(target, max_retries=10):
        for _ in range(10):
            msg = network.get_message()
            print(f"Got message {msg}")
        network.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:host=127.0.0.1;port=33333")
    args = parser.parse_args()
    main(args.target)
