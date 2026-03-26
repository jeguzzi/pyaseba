import collections.abc
import typing

class ArrayAccessOutOfBounds(Message):
    index: int
    pc: int
    size: int
    def __init__(self) -> None: ...


class BreakpointClear(CmdMessage):
    pc: int
    def __init__(self, dest: typing.SupportsInt | typing.SupportsIndex = ..., pc: typing.SupportsInt | typing.SupportsIndex = ...) -> None: ...


class BreakpointClearAll(CmdMessage):
    def __init__(self, dest: typing.SupportsInt | typing.SupportsIndex = ...) -> None: ...


class BreakpointSet(CmdMessage):
    pc: int
    def __init__(self, dest: typing.SupportsInt | typing.SupportsIndex = ..., pc: typing.SupportsInt | typing.SupportsIndex = ...) -> None: ...


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
    def __init__(self, dest: typing.SupportsInt | typing.SupportsIndex = ...) -> None: ...


class GetNodeDescription(CmdMessage):
    version: int
    def __init__(self, dest: typing.SupportsInt | typing.SupportsIndex = ...) -> None: ...


class GetVariables(CmdMessage):
    length: int
    start: int
    def __init__(self, dest: typing.SupportsInt | typing.SupportsIndex = ..., start: typing.SupportsInt | typing.SupportsIndex = ..., length: typing.SupportsInt | typing.SupportsIndex = ...) -> None: ...


class ListNodes(Message):
    version: int
    def __init__(self) -> None: ...


class LocalEventDescription(Message):
    def __init__(self) -> None: ...


class Message:
    source: int
    type: int


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
    def __init__(self, dest: typing.SupportsInt | typing.SupportsIndex = ...) -> None: ...


class Reboot(CmdMessage):
    def __init__(self, dest: typing.SupportsInt | typing.SupportsIndex = ...) -> None: ...


class Reset(CmdMessage):
    def __init__(self, dest: typing.SupportsInt | typing.SupportsIndex = ...) -> None: ...


class Run(CmdMessage):
    def __init__(self, dest: typing.SupportsInt | typing.SupportsIndex = ...) -> None: ...


class SetBytecode(CmdMessage):
    bytecode: list[int]
    start: int
    def __init__(self, dest: typing.SupportsInt | typing.SupportsIndex = ..., start: typing.SupportsInt | typing.SupportsIndex = ...) -> None: ...


class SetVariables(CmdMessage):
    start: int
    variables: list[int]
    def __init__(self, dest: typing.SupportsInt | typing.SupportsIndex = ..., start: typing.SupportsInt | typing.SupportsIndex = ..., variables: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex] = ...) -> None: ...


class Sleep(CmdMessage):
    def __init__(self, dest: typing.SupportsInt | typing.SupportsIndex = ...) -> None: ...


class Step(CmdMessage):
    def __init__(self, dest: typing.SupportsInt | typing.SupportsIndex = ...) -> None: ...


class Stop(CmdMessage):
    def __init__(self, dest: typing.SupportsInt | typing.SupportsIndex = ...) -> None: ...


class UserMessage(Message):
    data: list[int]
    def __init__(self, type: typing.SupportsInt | typing.SupportsIndex, data: collections.abc.Sequence[typing.SupportsInt | typing.SupportsIndex] = ...) -> None: ...


class Variables(Message):
    start: int
    variables: list[int]
    def __init__(self) -> None: ...


class WriteBytecode(CmdMessage):
    def __init__(self, dest: typing.SupportsInt | typing.SupportsIndex = ...) -> None: ...

