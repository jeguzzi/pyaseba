import argparse
import time

from pyaseba import Client
from pyaseba.client import Event


def cb(event: Event) -> None:
    print(f"Received {event}")


script = """
onevent send
emit echo event.args[0:2]
"""


def main(target: str) -> None:
    client = Client()
    client.add_event_callback(cb)
    if client.connect(target, max_retries=10):
        node = client.wait_node_connection(wait_ms=1000)
        if node:
            client.load_script(node=node,
                                script=script,
                                events=[("send", 3), ("echo", 3)])
            client.run(node=node)
            for i in range(5):
                client.emit_event(node, "send", [3, 2, i])
                time.sleep(0.5)
        client.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:host=127.0.0.1;port=33333")
    args = parser.parse_args()
    main(args.target)
