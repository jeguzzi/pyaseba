from .node import Node
from .shell import NodeShell

if __name__ == '__main__':
    node = Node(cached=False)
    node.connect(target='tcp:port=33333')
    NodeShell(node).cmdloop()
