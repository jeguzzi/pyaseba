import collections.abc
from . import Message

class ArrayAccessOutOfBounds(Message):
    index: int
    pc: int
    size: int
    def __init__(self) -> None: ...


class BreakpointClear(CmdMessage):
    pc: int
    def __init__(self, dest: int = ..., pc: int = ...) -> None: ...


class BreakpointClearAll(CmdMessage):
    def __init__(self, dest: int = ...) -> None: ...


class BreakpointSet(CmdMessage):
    pc: int
    def __init__(self, dest: int = ..., pc: int = ...) -> None: ...


class BreakpointSetResult(Message):
    pc: int
    success: int
    def __init__(self) -> None: ...


class CmdMessage(Message):
    dest: int


class Description(Message):
    name: str
    protocol_version: int
    def __init__(self) -> None: ...


class Disconnected(Message):
    def __init__(self) -> None: ...


class DivisionByZero(Message):
    pc: int
    def __init__(self) -> None: ...


class EventExecutionKilled(Message):
    pc: int
    def __init__(self) -> None: ...


class ExecutionStateChanged(Message):
    flags: int
    pc: int
    def __init__(self) -> None: ...


class GetDescription(Message):
    version: int
    def __init__(self) -> None: ...


class GetExecutionState(CmdMessage):
    def __init__(self, dest: int = ...) -> None: ...


class GetNodeDescription(CmdMessage):
    version: int
    def __init__(self, dest: int = ...) -> None: ...


class GetVariables(CmdMessage):
    length: int
    start: int
    def __init__(self, dest: int = ..., start: int = ..., length: int = ...) -> None: ...


class ListNodes(Message):
    version: int
    def __init__(self) -> None: ...


class LocalEventDescription(Message):
    def __init__(self) -> None: ...


class NamedVariableDescription(Message):
    def __init__(self) -> None: ...


class NativeFunctionDescription(Message):
    def __init__(self) -> None: ...


class NodePresent(Message):
    version: int
    def __init__(self) -> None: ...


class NodeSpecificError(Message):
    message: str
    pc: int
    def __init__(self) -> None: ...


class Pause(CmdMessage):
    def __init__(self, dest: int = ...) -> None: ...


class Reboot(CmdMessage):
    def __init__(self, dest: int = ...) -> None: ...


class Reset(CmdMessage):
    def __init__(self, dest: int = ...) -> None: ...


class Run(CmdMessage):
    def __init__(self, dest: int = ...) -> None: ...


class SetBytecode(CmdMessage):
    bytecode: list[int]
    start: int
    def __init__(self, dest: int = ..., start: int = ...) -> None: ...


class SetVariables(CmdMessage):
    start: int
    variables: list[int]
    def __init__(self, dest: int = ..., start: int = ..., variables: collections.abc.Sequence[int] = ...) -> None: ...


class Sleep(CmdMessage):
    def __init__(self, dest: int = ...) -> None: ...


class Step(CmdMessage):
    def __init__(self, dest: int = ...) -> None: ...


class Stop(CmdMessage):
    def __init__(self, dest: int = ...) -> None: ...


class UserMessage(Message):
    data: list[int]
    def __init__(self, type: int, data: collections.abc.Sequence[int] = ...) -> None: ...


class Variables(Message):
    start: int
    variables: list[int]
    def __init__(self) -> None: ...


class WriteBytecode(CmdMessage):
    def __init__(self, dest: int = ...) -> None: ...

