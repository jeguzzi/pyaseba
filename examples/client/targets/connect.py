import sys
from typing import Any

from pyaseba import Client


def main(protocol: str, **kwargs: Any) -> None:
    client = Client(port=33334)
    connection = client.connect(protocol, max_retries=0, **kwargs)
    if connection:
        print(f"Connected to {client.connections[connection]}")
    else:
        raise RuntimeError("Could not connect")
    client.close()


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
    try:
        main(protocol, **vs)
    except Exception as e:
        sys.exit(f"ERROR: {e}")
