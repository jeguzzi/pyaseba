import argparse
import time

from pyaseba.client.thymio import Thymio
from pyenki.behaviors import ThymioExplorerBehavior


def main(target: str) -> None:
    thymio = Thymio()
    behavior = ThymioExplorerBehavior()
    if thymio.connect(target=target):
        print("Start")
        thymio.set_controller(behavior, time_step=0.1)  # type: ignore[arg-type]
        try:
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            pass
        print("Stopped")
        thymio.call_leds_buttons(0, 0, 0, 0)
    else:
        print(f'Could not find a Thymio on {target}')
    thymio.close(reset=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:port=33333")
    args = parser.parse_args()
    main(args.target)
