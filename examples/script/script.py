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
        node = client.wait_node_connection(wait_ms=1000)
        if node is not None:
            client.load_script(node=node,
                                script=script,
                                events=[("c", 0), ("d", 2)])
            client.run(node)
            print(f'variables: {client.get_variables(node)}')
            print(f'events: {client.get_user_events(node)}')
            client.set_variable(node, "b", [10])
            client.emit_event(node, "c", [])
            e = client.get_event(node, "d", wait_ms=1000)
            if e:
                print(f"Got {e}")
            print(f"a = {client.get_variable(node, 'a', wait_ms=1000)}")
            print(f"b = {client.get_variable(node, 'b', wait_ms=1000)}")
        client.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:host=127.0.0.1;port=33333")
    args = parser.parse_args()
    main(args.target)
