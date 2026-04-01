import argparse

from pyaseba import Client

script = """
onevent send
emit echo event.args[0:2]
"""


def main(target: str) -> None:
    client = Client()
    if client.connect(target, max_retries=10):
        node = client.wait_node_connection(wait_ms=1000)
        if node:
            client.load_script(node=node,
                                script=script,
                                events=[("send", 3), ("echo", 3)])
            client.run(node=node)
            client.emit_event(node, "send", [3, 2, 1])
            e = client.get_event(node, "echo")
            print(f"Got {e}")
        client.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:host=127.0.0.1;port=33333")
    args = parser.parse_args()
    main(args.target)
