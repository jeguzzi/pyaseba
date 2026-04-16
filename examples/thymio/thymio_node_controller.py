import argparse
import time
from pyaseba.client.thymio import Thymio


def control(thymio: Thymio, dt: float) -> None:
    if thymio.prox_horizontal[2] > 2000:
        thymio.leds_top = [32, 0, 0]
        thymio.motor_left_target = 0
        thymio.motor_right_target = 0
    else:
        thymio.leds_top = [0, 32, 0]
        thymio.motor_left_target = 100
        thymio.motor_right_target = 100


def main(target: str) -> None:
    thymio = Thymio()
    if thymio.connect(target=target):
        thymio.set_controller(control, time_step=0.1)
        try:
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            pass
    else:
        print(f'Could not find a Thymio on {target}')
    thymio.close(reset=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:port=33333")
    args = parser.parse_args()
    main(args.target)
