import argparse

from pyaseba import Client

script = """
onevent send
emit echo event.args[0:2]
"""

script = """
onevent send
emit echo args[0:2]
"""


def main(target: str) -> None:
    client = Client()
    if client.connect(target, max_retries=10):
        node_id, conn = client.wait_node(wait_ms=1000)
        if conn:
            client.load_script(node_id=node_id,
                               script=script,
                               events={"send": 3, "echo": 3})
            client.cmd_run(node_id=node_id)
            client.emit_event(node_id, "send", [3, 2, 1])
            e = client.get_event(node_id, "echo", wait_ms=1000)
            print(f"Got {e}")
        client.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:host=127.0.0.1;port=33333")
    args = parser.parse_args()
    main(args.target)
