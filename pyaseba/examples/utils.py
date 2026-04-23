import logging
from pyaseba import set_logger_level
from typing import Any


def setup_logging(level: str = "INFO", name: str = "main") -> logging.Logger:
    kwargs: dict[str, Any] = {}
    try:
        from rich.logging import RichHandler

        kwargs['handlers'] = [RichHandler(show_path=False)]
    except ImportError:
        pass

    logging.basicConfig(level=level,
                        format="[%(name)s] %(message)s",
                        datefmt="[%X.%f]",
                        **kwargs)
    set_logger_level(level)
    return logging.getLogger(name)
