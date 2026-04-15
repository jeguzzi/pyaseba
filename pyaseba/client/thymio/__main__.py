from ..node.shell import NodeShell
from .thymio import Thymio

node = Thymio()
node.connect(target='ser:name=Thymio')
NodeShell(node).cmdloop()
