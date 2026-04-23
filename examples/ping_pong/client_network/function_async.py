import asyncio
import logging
import sys

from function_sync import RespondingNode, script
from ping_pong_async import main

if __name__ == '__main__':
    try:
        asyncio.run(main(node_cls=RespondingNode, script=script, sleep=0.1))
    except Exception as e:
        logging.error(str(e))
        sys.exit(1)
