from .._client_impl.msgs import *  # noqa: F403
from .._client_impl import msgs as _impl

type DescriptionFragment = _impl.Description | _impl.LocalEventDescription | _impl.NamedVariableDescription | _impl.NativeFunctionDescription
