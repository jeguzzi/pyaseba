import argparse
import logging
import sys
import time

from pyaseba.client.thymio import Thymio
from pyaseba.examples.utils import setup_logging

done = False


def control(thymio: Thymio, dt: float) -> None:
    global done
    if thymio.prox_horizontal[2] > 2000:
        done = True
        thymio.motor_left_target = 0
        thymio.motor_right_target = 0
        thymio.leds_top = [0, 0, 0]


def main(target: str) -> None:
    thymio = Thymio()
    if thymio.connect(target=target, start_mirroring=True):
        thymio.leds_top = [0, 32, 0]
        thymio.motor_left_target = 100
        thymio.motor_right_target = 100
        thymio.set_controller(control, event="prox")
        try:
            while not done:
                time.sleep(1)
        except KeyboardInterrupt:
            pass
    else:
        logging.error(f'Could not find a Thymio on {target}')
    thymio.close(reset=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="ser:name=Thymio")
    parser.add_argument('--log_level', default="INFO")
    args = parser.parse_args()
    setup_logging(args.log_level)
    try:
        main(args.target)
    except Exception as e:
        logging.error(str(e))
        sys.exit(1)
