import argparse

from pyaseba import Network


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
    network = Network()
    if network.connect(target, max_retries=10):
        node = network.wait_node_connection(wait_ms=1000)
        if node is not None:
            network.load_script(node=node,
                                script=script,
                                events=[("c", 0), ("d", 2)])
            network.run(node)
            print(f'variables: {network.get_variables(node)}')
            print(f'events: {network.get_user_events(node)}')
            network.set_variable(node, "b", [10])
            network.emit_event(node, "c", [])
            e = network.get_event(node, "d", wait_ms=1000)
            if e:
                print(f"Got {e}")
            print(f"a = {network.get_variable(node, 'a', wait_ms=1000)}")
            print(f"b = {network.get_variable(node, 'b', wait_ms=1000)}")
        network.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:host=127.0.0.1;port=33333")
    args = parser.parse_args()
    main(args.target)
