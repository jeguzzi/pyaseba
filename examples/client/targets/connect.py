import logging
import sys
from typing import Any

from pyaseba import Client
from pyaseba.examples.utils import setup_logging


def main(protocol: str, **kwargs: Any) -> None:
    with Client(port=33334) as client:
        connection = client.connect(protocol, max_retries=0, **kwargs)
        if connection:
            logging.info(f"Connected to {client.connections[connection]}")
        else:
            raise RuntimeError("Could not connect")


if __name__ == '__main__':
    if len(sys.argv) == 1:
        sys.argv.append('tcp')
        sys.argv.append('--port')
        sys.argv.append('33333')
    protocol = sys.argv[1]
    vs = {
        k[2:]: v
        for k, v in zip(sys.argv[2::2], sys.argv[3::2], strict=False)
    }
    setup_logging("INFO")
    try:
        main(protocol, **vs)
    except Exception as e:
        logging.error(str(e))
        sys.exit(1)
