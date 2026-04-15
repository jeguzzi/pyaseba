from .node import Node
from .shell import NodeShell


node = Node(cached=False)
node.connect(target='tcp:port=33333')
NodeShell(node).cmdloop()
