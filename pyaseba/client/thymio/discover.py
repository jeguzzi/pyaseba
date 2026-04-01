import time

from zeroconf import ServiceBrowser, ServiceListener, Zeroconf


def discover(timeout: float = 1, min_number: int = 1) -> set[str]:
    targets: set[str] = set()
    with Zeroconf() as zeroconf:
        listener = ThymioListener(targets)
        _ = ServiceBrowser(zeroconf, "_aseba._tcp.local.", listener)
        step = 1e-2
        i = int(timeout / step)
        while i > 0 and len(targets) < min_number:
            i -= 1
            time.sleep(1e-2)
    return targets


class ThymioListener(ServiceListener):

    def __init__(self, targets: set[str]):
        super().__init__()
        self.targets = targets

    def update_service(self, zc: Zeroconf, type_: str, name: str) -> None:
        pass

    def remove_service(self, zc: Zeroconf, type_: str, name: str) -> None:
        pass

    def add_service(self, zc: Zeroconf, type_: str, name: str) -> None:
        info = zc.get_service_info(type_, name)
        if not info:
            return
        type = info.properties.get(b'type', b'')
        if type != b'Thymio II':
            return
        port = info.port
        local_addresses = [a for a in info._ipv4_addresses if a.is_loopback]
        if local_addresses:
            address = str(local_addresses[-1])
        else:
            address = str(info._ipv4_addresses[-1])
        target = f'tcp:host={address};port={port}'
        self.targets.add(target)
