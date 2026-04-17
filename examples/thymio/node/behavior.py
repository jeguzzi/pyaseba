import argparse
import sys
import time
from collections.abc import Callable

import thymio_behaviors
from pyaseba.client.node import EventSpecUpdate
from pyaseba.client.thymio import Thymio


def main(target: str, names: str) -> None:
    enable_prox_comm = False
    events: dict[str, EventSpecUpdate] = {}
    behaviors: list[Callable[[Thymio, float], None]] = []
    for name in names:
        if name == 'explorer':
            behaviors.append(thymio_behaviors.ExplorerBehavior())
        elif name == 'follower':
            behaviors.append(thymio_behaviors.FollowerBehavior())
            enable_prox_comm = True
        elif name == 'line':
            behaviors.append(thymio_behaviors.LineFollowingBehavior())
        elif name == 'prox':
            behaviors.append(thymio_behaviors.LEDProxBehavior())
        elif name == 'acc':
            behaviors.append(thymio_behaviors.AccBehavior())
            events['acc'] = {'window': 1}
        elif name == 'led_acc':
            behaviors.append(thymio_behaviors.LEDAccBehavior())
            events['acc'] = {'window': 1}
        elif name == 'prox_comm':
            behaviors.append(thymio_behaviors.LEDProxCommBehavior())
            enable_prox_comm = True
        elif name == 'led_buttons':
            behaviors.append(thymio_behaviors.LEDButtonsBehavior())
        elif name == 'sound_buttons':
            behaviors.append(thymio_behaviors.SoundButtonsBehavior())
        else:
            raise ValueError(f"Unknown behavior {name}")
    if not behaviors:
        raise ValueError("No behavior selected")
    behavior = thymio_behaviors.Chain(*behaviors)
    thymio = Thymio(record_prox_comm=enable_prox_comm)
    if events:
        thymio.configure_events(**events)
    if thymio.connect(target=target):
        print(f"Starting {' - '.join(names)}")
        thymio.call_prox_comm_enable(enable_prox_comm)
        thymio.set_controller(behavior, time_step=0.1)
        try:
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            pass
        thymio.call_leds_buttons(0, 0, 0, 0)
    else:
        print(f'Could not find a Thymio on {target}')
    print('Closing')
    thymio.close(reset=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--behavior',
                        nargs='*',
                        default="explorer",
                        choices=[
                            "explorer", "follower", "acc", "line", "prox",
                            "prox_comm", "led_buttons", "sound_buttons",
                            "led_acc"
                        ])
    parser.add_argument('--target', default="ser:name=Thymio")
    args = parser.parse_args()
    try:
        main(args.target, args.behavior)
    except Exception as e:
        sys.exit(f"ERROR: {e}")
