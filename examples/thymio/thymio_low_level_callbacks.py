import time
import argparse
from pyaseba.client import Client, Event


def main(target: str) -> None:
    client = Client()
    if client.connect(target):
        node_id, conn = client.wait_node(wait_ms=5000)
        if conn:
            print(f'node {node_id}')
            done = False
            script = """
onevent prox
emit proxh prox.horizontal
"""
            client.load_script(node_id=node_id,
                               script=script,
                               events={"proxh": 7})
            desc = client.get_description(node_id)
            assert desc
            index, _ = desc.variables['motor.left.target']

            def cb(event: Event) -> None:
                nonlocal done
                if event.name == 'proxh' and event.source == node_id:
                    if event.data[2] > 2000:
                        done = True

            client.add_event_callback(cb)
            client.cmd_run(node_id)
            client.set_variable_by_index(node_id, index, [100, 100])
            client.set_variable(node_id, "leds.top", [0, 32, 0])
            while not done:
                time.sleep(0.1)
            client.cmd_reset(node_id)
            time.sleep(0.2)
        else:
            print('no node')
    time.sleep(0.2)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:port=33333")
    args = parser.parse_args()
    main(args.target)
