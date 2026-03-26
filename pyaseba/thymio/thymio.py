from ..node import Node
from ..node_async import NodeAsync


class Thymio(Node):

    events = {
        "prox": ["prox.horizontal", "prox.ground.delta"],
        "button.backward": ["button.backward"],
        "button.left": ["button.left"],
        "button.center": ["button.center"],
        "button.forward": ["button.forward"],
        "button.right": ["button.right"],
        "tap": ["acc._tap"],
        "mic": ["mic.intensity"],
        "rc5": ["rc5.address", "rc5.command"],
        "timer0": [],
        "timer1": []
    }
    function_prefixes = ('leds', '_leds', 'sound', 'prox')
    function_exclude = ("wave", )
    target = "ser:name=Thymio"

    def _stop(self) -> None:
        super()._stop()
        self.set("motor.left.target", 0, cached=False)
        self.set("motor.right.target", 0, cached=False)


class ThymioAsync(Thymio, NodeAsync):
    pass
