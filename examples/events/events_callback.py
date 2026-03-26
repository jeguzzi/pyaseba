import argparse
import time

from pyaseba import Network, Event


def cb(event: Event) -> None:
    print(f"Received {event}")


script = """
onevent send
emit echo event.args[0:2]
"""


def main(target: str) -> None:
    network = Network()
    # TODO: fails (prob. because when it tries to get a name)
    # network.add_event_callback(cb)
    if network.connect(target, max_retries=10):
        node = network.wait_node_connection(wait_ms=1000)
        if node:
            network.add_event_callback(cb)
            network.load_script(node=node,
                                script=script,
                                events=[("send", 3), ("echo", 3)])
            network.run(node=node)
            for i in range(5):
                network.emit_event(node, "send", [3, 2, i])
                time.sleep(0.5)
        network.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:host=127.0.0.1;port=33333")
    args = parser.parse_args()
    main(args.target)
