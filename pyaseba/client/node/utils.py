import re
from collections.abc import Collection


def int16(x: int) -> int:
    if x > 2**15:
        x -= 2**16
    return x


def matches(name: str, include: Collection[str],
            exclude: Collection[str]) -> bool:
    return (any(re.findall(e, name) for e in include)
            and not any(re.findall(e, name) for e in exclude))
