from collections.abc import Callable
from typing import ParamSpec

from thymio_behaviors import Behavior, Callback

from ..node import Node, NodeAsync

P = ParamSpec('P')


class Thymio(Node):
    def call__leds_set(self, *args: int) -> None: ...
    def call_sound_record(self, *args: int) -> None: ...
    def call_sound_play(self, *args: int) -> None: ...
    def call_sound_replay(self, *args: int) -> None: ...
    def call_sound_system(self, *args: int) -> None: ...
    def call_leds_circle(self, *args: int) -> None: ...
    def call_leds_top(self, *args: int) -> None: ...
    def call_leds_bottom_left(self, *args: int) -> None: ...
    def call_leds_bottom_right(self, *args: int) -> None: ...
    def call_sound_freq(self, *args: int) -> None: ...
    def call_leds_buttons(self, *args: int) -> None: ...
    def call_leds_prox_h(self, *args: int) -> None: ...
    def call_leds_prox_v(self, *args: int) -> None: ...
    def call_leds_rc(self, *args: int) -> None: ...
    def call_leds_sound(self, *args: int) -> None: ...
    def call_leds_temperature(self, *args: int) -> None: ...
    def call_prox_comm_enable(self, *args: int) -> None: ...
    def call_sound_duration(self, *args: int) -> None: ...
    _fwversion: list[int]
    _id: int
    _imot: list[int]
    _integrator: list[int]
    _productId: int
    _vbat: list[int]
    acc: list[int]
    acc__tap: int
    button_backward: int
    button_center: int
    button_forward: int
    button_left: int
    button_right: int
    buttons__mean: list[int]
    buttons__noise: list[int]
    buttons__raw: list[int]
    event_args: list[int]
    event_source: int
    leds_bottom_left: list[int]
    leds_bottom_right: list[int]
    leds_circle: list[int]
    leds_top: list[int]
    mic__mean: int
    mic_intensity: int
    mic_threshold: int
    motor_left_pwm: int
    motor_left_speed: int
    motor_left_target: int
    motor_right_pwm: int
    motor_right_speed: int
    motor_right_target: int
    prox_comm_rx: int
    prox_comm_rx__align_tol: list[int]
    prox_comm_rx__intensities: list[int]
    prox_comm_rx__lead: list[int]
    prox_comm_rx__payloads: list[int]
    prox_comm_rx__tol: list[int]
    prox_comm_rx__trail: list[int]
    prox_comm_tx: int
    prox_ground_ambiant: list[int]
    prox_ground_delta: list[int]
    prox_ground_reflected: list[int]
    prox_horizontal: list[int]
    rc5_address: int
    rc5_command: int
    sd_present: int
    temperature: int
    timer_period: list[int]
    on_prox: Callable[[Thymio], None] | None
    on_button_backward: Callable[[Thymio], None] | None
    on_button_left: Callable[[Thymio], None] | None
    on_button_center: Callable[[Thymio], None] | None
    on_button_forward: Callable[[Thymio], None] | None
    on_button_right: Callable[[Thymio], None] | None
    on_tap: Callable[[Thymio], None] | None
    on_mic: Callable[[Thymio], None] | None
    on_rc5: Callable[[Thymio], None] | None
    on_timer0: Callable[[Thymio], None] | None
    on_timer1: Callable[[Thymio], None] | None

    @property
    def prox_comm_buffer(self) -> list[tuple[int, list[int]]]: ...

    def __init__(self,
                 cached: bool = ...,
                 record_prox_comm: bool = ...,
                 enable_prox_comm: bool = ...) -> None: ...

    def set_behavior(self,
                     behavior: Behavior,
                     time_step: float = ...,
                     event: str = ...) -> None: ...

    def apply(self, callback: Callback[P], *args: P.args,
              **kwargs: P.kwargs) -> None: ...


class ThymioAsync(NodeAsync, Thymio): ...
