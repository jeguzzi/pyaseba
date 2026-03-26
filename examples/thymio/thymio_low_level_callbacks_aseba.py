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
motor.left.target = 100
motor.right.target = 100
leds.top = [0, 32, 0]
onevent prox
if prox.horizontal[2] > 2000 then
  emit done
end
"""
            network.load_script(node=node,
                                script=script,
                                events=[("done", 0)])

            def cb(event: Event) -> None:
                nonlocal done
                if event.name == 'done' and event.source == node:
                    done = True

            network.add_event_callback(cb)
            network.run(node=node)
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
