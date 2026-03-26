import time
import argparse
from pyaseba import connect, Event


def main(target: str) -> None:
    with connect(target) as network:
        node = network.wait_node_connection(wait_ms=5000)
        if node:
            print(f'node {node}')
            done = False
            script = """
onevent prox
emit proxh prox.horizontal
"""
            network.load_script(node=node,
                                script=script,
                                events=[("proxh", 7)])
            desc = network.get_description(node)
            assert(desc is not None)
            index, _ = desc._variables_map['motor.left.target']

            def cb(event: Event) -> None:
                nonlocal done
                if event.name == 'proxh' and event.source == node:
                    if event.data[2] > 2000:
                        done = True

            network.add_event_callback(cb)
            network.run(node=node)
            network.set_variables(node, index, [100, 100])
            network.set_variable(node, "leds.top", [0, 32, 0])
            while not done:
                time.sleep(0.1)
            network.reset(node=node)
            time.sleep(0.2)
        else:
            print('no node')
    time.sleep(0.2)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:host=127.0.0.1;port=33333")
    args = parser.parse_args()
    main(args.target)
