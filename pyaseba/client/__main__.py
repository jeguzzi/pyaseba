from pyaseba.client import Client
import cmd


class ClientShell(cmd.Cmd):
    intro = 'Welcome to the pyaseba client shell. Type help or ? to list commands.\n'
    prompt = '(client) '

    def __init__(self) -> None:
        super().__init__()
        self.client = Client()

    def do_connect(self, arg: str) -> None:
        'Connect to a target:  connect [TARGET]'
        conn = self.client.connect(arg or "tcp:port=33333")
        print(conn)

    def do_connections(self, arg: str) -> None:
        'Print the active connections:  connections'
        print(self.client.connections)

    def do_node_ids(self, arg: str) -> None:
        'Print the connected node ids:  node_ids'
        print(self.client.get_node_ids())

    def node_ids(self, line: str = '') -> list[str]:
        ids = set(
            [i for _, ids in self.client.get_node_ids().items() for i in ids])
        return [str(i) for i in ids if line in str(i)]

    def complete_description(self, text: str, line: str, begidx: int,
                             endidx: int) -> list[str]:
        return self.node_ids(text)

    def do_description(self, arg: str) -> None:
        'Print the description of a node:  description NODE_ID'
        try:
            v, *_ = arg.split(' ')
            node_id = int(v)
            desc = self.client.get_description(node_id)
            print(desc)
        except Exception:
            pass

    def do_wait_nodes(self, arg: str) -> None:
        'Wait until nodes are discovered:  wait_nodes [NUMBER]'
        number = 1
        try:
            v, *_ = arg.split(' ')
            number = int(v)
        except Exception:
            pass
        node_id, conn = self.client.wait_nodes(number=number)
        print(node_id, conn)

    def complete_variables(self, text: str, line: str, begidx: int,
                           endidx: int) -> list[str]:
        return self.node_ids(text)

    def do_variables(self, arg: str) -> None:
        'Print the name of all variables defined by a node:  variables NODE_ID'
        try:
            v, *_ = arg.split(' ')
            node_id = int(v)
            desc = self.client.get_description(node_id)
            if desc:
                print(list(desc.variables))
        except Exception:
            pass

    def complete_events(self, text: str, line: str, begidx: int,
                        endidx: int) -> list[str]:
        return self.node_ids(text)

    def do_events(self, arg: str) -> None:
        'Print the name of all events defined by a node:  events NODE_ID'
        try:
            v, *_ = arg.split(' ')
            node_id = int(v)
            desc = self.client.get_description(node_id)
            if desc:
                print(list(desc.local_events))
        except Exception:
            pass

    def complete_functions(self, text: str, line: str, begidx: int,
                           endidx: int) -> list[str]:
        return self.node_ids(text)

    def do_functions(self, arg: str) -> None:
        'Print the name of all functions defined by a node:  functions NODE_ID'
        try:
            v, *_ = arg.split(' ')
            node_id = int(v)
            desc = self.client.get_description(node_id)
            if desc:
                print(list(desc.functions))
        except Exception:
            pass

    def complete_get_variable(self, text: str, line: str, begidx: int,
                              endidx: int) -> list[str]:
        if begidx == 13:
            return self.node_ids(text)
        else:
            options = []
            try:
                _, v, *_ = line.split()
                node_id = int(v)
                desc = self.client.get_description(node_id)
                if desc:
                    options = list(desc.variables)
                return [i for i in options if text in i]
            except Exception:
                return []

    def do_get_variable(self, arg: str) -> None:
        'Get the value of a variable: get_variable NODE_ID NAME'
        try:
            v, name, *_ = arg.split(' ')
            node_id = int(v)
            value = self.client.get_variable(node_id, name)
            print(value)
        except Exception:
            pass

    def complete_get_all_variables(self, text: str, line: str, begidx: int,
                                   endidx: int) -> list[str]:
        return self.node_ids(text)

    def do_get_all_variables(self, arg: str) -> None:
        'Get the value of all variable: get_variables NODE_ID'
        try:
            v, *_ = arg.split(' ')
            node_id = int(v)
            value = self.client.get_all_variables(node_id)
            print(value)
        except Exception:
            pass

    def complete_set_variable(self, text: str, line: str, begidx: int,
                              endidx: int) -> list[str]:
        return self.complete_get_variable(text, line, begidx, endidx)

    def do_set_variable(self, arg: str) -> None:
        'Set the value of a variable: set_variable NODE_ID NAME <VALUE 1> <VALUE 2> ...'
        try:
            v, name, *vs = arg.split(' ')
            node_id = int(v)
            value = [int(x) for x in vs]
            print(node_id, name, value)
            self.client.set_variable(node_id, name, value)
        except Exception:
            pass

    def do_get_message(self, arg: str) -> None:
        'Wait until it receives a message and print it: get_message'
        msg = self.client.get_message()
        print(msg)

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
        self.client.close()


ClientShell().cmdloop()
