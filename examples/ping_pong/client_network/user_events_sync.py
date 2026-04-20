import sys
from pyaseba.network import Node
from ping_pong_sync import main

script = """
onevent pong
emit ping
"""


if __name__ == '__main__':
    try:
        main(node_cls=Node, script=script, sleep=0.1)
    except Exception as e:
        sys.exit(f"ERROR: {e}")
