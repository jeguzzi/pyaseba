from ..node.shell import NodeShell
from .thymio import Thymio

if __name__ == '__main__':
    node = Thymio()
    node.connect(target='ser:name=Thymio')
    shell = NodeShell(node)
    shell.prompt = '(thymio)'
    shell.cmdloop()
