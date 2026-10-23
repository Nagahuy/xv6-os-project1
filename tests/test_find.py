#!/usr/bin/env python3
"""Run with /usr/bin/python3 tests/test_find.py; requires python3-pexpect."""

import subprocess
import unittest

from test_shell import Console, ROOT


class FindTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        subprocess.run(["make", "-s", "kernel/kernel", "fs.img"], cwd=ROOT, check=True)

    def test_deep_tree_and_exec(self):
        for depth in (12, 20, 58):
            with self.subTest(depth=depth):
                console = Console("find-depth-%d" % depth)
                try:
                    commands = ["mkdir deep", "cd deep"]
                    commands += ["/mkdir a", "cd a"] * depth
                    commands += ["/echo > leaf", "cd /",
                                 "echo > deep/leaf", "echo > leaf"]
                    for command in commands:
                        output = console.command(command)
                        self.assertNotRegex(output, r"failed|cannot|panic|usertrap")
                    expected = ["./deep/" + "a/" * depth + "leaf",
                                "./deep/leaf", "./leaf"]
                    output = console.command("find . leaf")
                    self.assertEqual(output.splitlines()[1:], expected)
                    output = console.command("find . leaf -exec echo MATCH")
                    self.assertEqual(output.splitlines()[1:],
                                     ["MATCH " + path for path in expected])
                finally:
                    console.close()


if __name__ == "__main__":
    unittest.main(verbosity=2)
