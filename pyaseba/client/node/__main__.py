import argparse

from .node import Node
from .shell import NodeShell

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', default="tcp:port=33333")
    args = parser.parse_args()
    node = Node(cached=False)
    if not node.connect(args.target):
        print(f"Could not connect to {args.target}: exiting ...")
    NodeShell(node).cmdloop()
