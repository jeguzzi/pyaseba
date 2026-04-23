import logging
import sys

from pyaseba.network import Node

from ping_pong_sync import main

script = """
onevent pong
recevied_pong = 1

onevent response
emit ping
"""


class RespondingNode(Node):
    variables = {'recevied_pong': 1}
    events = {'response': 'emitted when responding'}

    def tick(self, time_step: float) -> None:
        if any(self.get("recevied_pong")):
            self.set("recevied_pong", [0])
            logging.info("ping")
            self.emit("response")


if __name__ == '__main__':
    try:
        main(node_cls=RespondingNode, script=script, sleep=0.1)
    except Exception as e:
        logging.error(str(e))
        sys.exit(1)
