import argparse

from pyaseba import Client


script = """
var a
var b
a = 0
b = 0
onevent c
a = b + 1
emit d [a, b]
"""


def main(target: str) -> None:
    client = Client()
    if client.connect(target, max_retries=10):
        node_id, conn = client.wait_node(wait_ms=1000)
        if conn:
            client.load_script(node_id=node_id,
                               script=script,
                               events={"c": 0, "d": 2})
            client.cmd_run(node_id)
            desc = client.get_description(node_id)
            assert desc
            print(f'variables: {desc.variables}')
            print(f'events: {desc.user_events}')
            client.set_variable(node_id, "b", [10])
            client.emit_event(node_id, "c", [])
            e = client.get_event(node_id, "d", wait_ms=1000)
            if e:
                print(f"Got {e}")
            print(f"a = {client.get_variable(node_id, 'a', wait_ms=1000)}")
            print(f"b = {client.get_variable(node_id, 'b', wait_ms=1000)}")
        client.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:host=127.0.0.1;port=33333")
    args = parser.parse_args()
    main(args.target)
