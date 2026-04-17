import time
import argparse
import sys
from pyaseba.client import Client, Event


def main(target: str) -> None:
    client = Client()
    if client.connect(target):
        node_id, conn = client.wait_node(wait_ms=5000)
        if conn:
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
            client.load_script(node_id=node_id,
                               script=script,
                               events={"done": 0})

            def cb(event: Event) -> None:
                nonlocal done
                if event.name == 'done' and event.source == node_id:
                    done = True

            client.add_event_callback(cb)
            client.cmd_run(node_id)
            while not done:
                time.sleep(0.1)
            client.cmd_reset(node_id)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="ser:name=Thymio")
    args = parser.parse_args()
    try:
        main(args.target)
    except Exception as e:
        sys.exit(f"ERROR: {e}")
