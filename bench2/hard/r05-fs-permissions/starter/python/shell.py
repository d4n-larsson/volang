"""The state of the shell: the tree and the current directory."""
import nodes


class Shell:
    def __init__(self):
        self.root = nodes.new_dir()
        # The current directory as a list of names from the root.
        self.cwd = []
