import logging
import sys

from pyaseba.network import Node

from ping_pong_sync import main

script = """
onevent pong
call respond()

onevent response
emit ping
"""


class RespondingNode(Node):
    functions = {'respond': ('', [])}
    function_include = [r'.*', ]
    events = {'response': 'emitted by respond'}

    def respond(self) -> None:
        logging.info("ping")
        self.emit("response")


if __name__ == '__main__':
    try:
        main(node_cls=RespondingNode, script=script, sleep=0.1)
    except Exception as e:
        logging.error(str(e))
        sys.exit(1)
