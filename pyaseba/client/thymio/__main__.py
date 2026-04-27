import argparse

from ..node.shell import NodeShell
from .thymio import Thymio

if __name__ == '__main__':
    node = Thymio()
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="ser:name=Thymio")
    args = parser.parse_args()
    node.connect(target=args.target)
    shell = NodeShell(node)
    shell.prompt = '(thymio)'
    shell.cmdloop()
