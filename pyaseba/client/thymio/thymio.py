from ..node import EventSpec, Node, NodeAsync
from .._client_impl import Event


class Thymio(Node):
    """
    High-level interface to a remote Thymio.

    >>> thymio = Thymio()
    >>> thymio.connect()
    >>> thymio.leds_top = [32, 0, 32]
    >>> thymio.prox_horizontal
    [0, 0, 1215, 1563, 2065, 0, 0]

    that can run an offboard Python controller like

    >>> def control(thymio: Thymio) -> None
            if thymio.prox_horizontal[2] > 2000:
                thymio.motor_left_target = 0
                thymio.motor_right_target = 0
            else:
                thymio.motor_left_target = 100
                thymio.motor_right_target = 100
    >>>
    >>> thymio.set_controller(control)
    """

    events = {
        "prox":
        EventSpec(variables=["prox.horizontal", "prox.ground.delta"],
                  use_counter=True),
        "prox.comm":
        EventSpec(variables=["prox.comm.rx", "prox.comm.rx._intensities"],
                  use_counter=True,
                  external_counter="event_prox"),
        "button.backward":
        EventSpec(["button.backward"]),
        "button.left":
        EventSpec(["button.left"]),
        "button.center":
        EventSpec(["button.center"]),
        "button.forward":
        EventSpec(["button.forward"]),
        "button.right":
        EventSpec(["button.right"]),
        "tap":
        EventSpec(["acc._tap"]),
        "mic":
        EventSpec(["mic.intensity"]),
        "rc5":
        EventSpec(["rc5.address", "rc5.command"]),
        "timer0":
        EventSpec([]),
        "timer1":
        EventSpec([])
    }
    function_include = ('leds', '_leds', 'sound', 'prox')
    function_exclude = (r"wave", )
    default_target = "ser:name=Thymio"
    properties = [
        '_fwversion', '_id', '_imot', '_integrator', '_productId', '_vbat',
        'acc', 'acc._tap', 'button.backward', 'button.center',
        'button.forward', 'button.left', 'button.right', 'buttons._mean',
        'buttons._noise', 'buttons._raw', 'event.args', 'event.source',
        'leds.bottom.left', 'leds.bottom.right', 'leds.circle', 'leds.top',
        'mic._mean', 'mic.intensity', 'mic.threshold', 'motor.left.pwm',
        'motor.left.speed', 'motor.left.target', 'motor.right.pwm',
        'motor.right.speed', 'motor.right.target', 'prox.comm.rx',
        'prox.comm.rx._align_tol', 'prox.comm.rx._intensities',
        'prox.comm.rx._lead', 'prox.comm.rx._payloads', 'prox.comm.rx._tol',
        'prox.comm.rx._trail', 'prox.comm.tx', 'prox.ground.ambiant',
        'prox.ground.delta', 'prox.ground.reflected', 'prox.horizontal',
        'rc5.address', 'rc5.command', 'sd.present', 'temperature',
        'timer.period'
    ]
    functions = [
        '_leds.set', 'sound.record', 'sound.play', 'sound.replay',
        'sound.system', 'leds.circle', 'leds.top', 'leds.bottom.left',
        'leds.bottom.right', 'sound.freq', 'leds.buttons', 'leds.prox.h',
        'leds.prox.v', 'leds.rc', 'leds.sound', 'leds.temperature',
        'prox.comm.enable', 'sound.duration'
    ]
    control_event = "timer0"

    def __init__(self,
                 cached: bool = False,
                 record_prox_comm: bool = False) -> None:
        """
        Constructs a new instance.

        :param cached:  The default value of ``cached``
           used by :py:meth:`set`, :py:meth:`get`, and :py:meth:`get_all`
        :param record_prox_comm:  Whether to record all ``prox.comm`` events emitted in-between
           ``prox`` events.
        """
        super().__init__(cached=cached)
        self._should_record_prox_comm = record_prox_comm
        self.prox_comm_buffer: list[tuple[int, list[int]]] = []
        self._next_prox_comm_buffers: dict[int, list[tuple[int,
                                                           list[int]]]] = []
        self._prox_comm_counter: int = 0

    @property
    def record_prox_comm(self) -> bool:
        return self._should_record_prox_comm

    @record_prox_comm.setter
    def record_prox_comm(self, value: bool) -> None:
        self._should_record_prox_comm = value
        if not value:
            self.prox_comm_buffer = []
            self._next_prox_comm_buffers = {}

    def _update_prox_counter(self, counter: int) -> int:
        diff = (counter - self._prox_comm_counter) % 0xFFFF
        self._prox_comm_counter += diff
        return self._prox_comm_counter

    def _append_to_prox_comm_buffer(self, counter: int,
                                    data: tuple[int, list[int]]) -> None:
        # print('_append_to_prox_comm_buffer', counter, data)
        counter = self._update_prox_counter(counter)
        if counter not in self._next_prox_comm_buffers:
            if counter - 1 in self._next_prox_comm_buffers:
                # print('.', self._node_id, counter - 1)
                self.prox_comm_buffer = self._next_prox_comm_buffers[counter -
                                                                     1]
            self._next_prox_comm_buffers = {counter: [data]}
        else:
            self._next_prox_comm_buffers[counter].append(data)
        # print('->', self._next_prox_comm_buffers, counter)

    def _update_prox_comm_buffer(self, counter: int) -> None:
        counter = self._update_prox_counter(counter)
        if counter in self._next_prox_comm_buffers:
            # print('*', self._node_id, counter)
            self.prox_comm_buffer = self._next_prox_comm_buffers[counter]
        else:
            self.prox_comm_buffer = []
        self._next_prox_comm_buffers = {}

    def _extra_event_cb(self, event: Event) -> None:
        # print(event)
        if self._should_record_prox_comm:
            if event.name == "event_prox.comm":
                rx = self._variable_values["prox.comm.rx"]
                intensities = self._variable_values[
                    "prox.comm.rx._intensities"]
                counter = event.data[-1]
                self._append_to_prox_comm_buffer(counter, (rx[0], intensities))
            if event.name == "event_prox":
                counter = event.data[-1]
                self._update_prox_comm_buffer(counter)

    def _stop(self) -> None:
        super()._stop()
        # TODO: Not really needed if we reset
        self.set("motor.left.target", 0, cached=False)
        self.set("motor.right.target", 0, cached=False)

    def set_control_period(self, time_step: float, event: str = "") -> None:
        if time_step >= 0 and event in ("timer0", "timer1"):
            steps = int(time_step * 1000)
            periods = self.get("timer.period") or [0, 0]
            if event == "timer0":
                periods[0] = steps  # type: ignore[index]
            else:
                periods[1] = steps  # type: ignore[index]
            self.set("timer.period", periods, cached=False)


class ThymioAsync(NodeAsync, Thymio):
    """
    Asynchronous version of :py:class:`pyaseba.client.thymio.Thymio`
    based on :py:class:`pyaseba.client.NodeAsync`.
    """

    def __init__(self,
                 cached: bool = False,
                 record_prox_comm: bool = False) -> None:
        Thymio.__init__(self, cached=cached, record_prox_comm=record_prox_comm)
