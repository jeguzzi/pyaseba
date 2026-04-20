import asyncio
import sys

from pyaseba.network import Node

from ping_pong_async import main
from user_events_sync import script

if __name__ == '__main__':
    try:
        asyncio.run(main(node_cls=Node, script=script, sleep=0.1))
    except Exception as e:
        sys.exit(f"ERROR: {e}")
