from .node import Node
import cmd


class NodeShell(cmd.Cmd):
    intro = 'Welcome to the pyaseba node shell.   Type help or ? to list commands.\n'
    prompt = '(node) '

    def __init__(self, node: Node) -> None:
        super().__init__()
        self.node = node

    def do_description(self, arg: str) -> None:
        'Print the description:  description'
        print(self.node.description)

    def do_variables(self, arg: str) -> None:
        'Print the name of all variables:  variables'
        desc = self.node.description
        if desc:
            print(list(desc.variables))

    def do_events(self, arg: str) -> None:
        'Print the name of all local events:  events'
        desc = self.node.description
        if desc:
            print(list(desc.local_events))

    def do_functions(self, arg: str) -> None:
        'Print the name of all functions:  functions'
        desc = self.node.description
        if desc:
            print(list(desc.functions))

    def do_mirrored(self, arg: str) -> None:
        'Print the name of all mirrored events:  mirrored'
        print(list(self.node.mirrored_events))

    def do_exposed(self, arg: str) -> None:
        'Print the name of all exposed functions:  exposed'
        print(list(self.node.exposed_functions))

    def complete_get(self, text: str, line: str, begidx: int,
                     endidx: int) -> list[str]:
        desc = self.node.description
        if desc:
            options = list(desc.variables)
            return [i for i in options if text in i]
        return []

    def do_get(self, arg: str) -> None:
        'Get the value of variables: get [NAME]'
        if arg:
            name, *_ = arg.split()
            print(self.node.get(name))
        else:
            print(self.node.get_all())

    def complete_set(self, text: str, line: str, begidx: int,
                     endidx: int) -> list[str]:
        return self.complete_get(text, line, begidx, endidx)

    def do_set(self, arg: str) -> None:
        'Set the value of variables: get NAME VALUE_1 VALUE_2 ...'
        try:
            name, *xs = arg.split()
            value = [int(x) for x in xs]
            self.node.set(name, value, cached=False)
        except Exception:
            pass

    def complete_call(self, text: str, line: str, begidx: int,
                      endidx: int) -> list[str]:
        options = list(self.node.exposed_functions)
        return [i for i in options if text in i]

    def do_call(self, arg: str) -> None:
        'Call a function: call NAME ARG_1 ARG_2 ...'
        try:
            name, *xs = arg.split()
            value = [int(x) for x in xs]
            self.node.call(name, *value)
        except Exception:
            pass

    def do_script(self, arg: str) -> None:
        'Print the Aseba script: script'
        print(self.node.script)

    def complete_wait(self, text: str, line: str, begidx: int,
                      endidx: int) -> list[str]:
        options = list(self.node.mirrored_events)
        return [i for i in options if text in i]

    def do_wait(self, arg: str) -> None:
        'Wait for an event: wait NAME'
        try:
            name, *_ = arg.split()
            self.node.wait(name, wait_ms=10000)
        except Exception:
            pass

    def do_quit(self, arg: str) -> bool:
        'Stop and exit:  quit'
        self.close()
        return True

    def do_exit(self, arg: str) -> bool:
        'Stop and exit:  exit'
        self.close()
        return True

    def precmd(self, line: str) -> str:
        return line

    def close(self) -> None:
        self.node.close()
