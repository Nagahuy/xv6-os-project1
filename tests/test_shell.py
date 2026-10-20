#!/usr/bin/env python3
"""Real serial-console integration tests; Tab/arrows are sent BEFORE Enter.

Run on Linux: python3 tests/test_shell.py
Requires make, a RISC-V toolchain, qemu-system-riscv64 and python3-pexpect.
Each test boots its own snapshot, leaving fs.img unchanged.
"""

from pathlib import Path
import subprocess
import tempfile
import unittest

import pexpect

ROOT = Path(__file__).resolve().parents[1]
LOGS = ROOT / "test-results"


class Console:
    def __init__(self, name, image="fs.img"):
        LOGS.mkdir(exist_ok=True)
        self.log = (LOGS / (name + ".log")).open("w", encoding="utf-8")
        self.child = pexpect.spawn(
            "qemu-system-riscv64",
            ["-machine", "virt", "-bios", "none", "-kernel", "kernel/kernel",
             "-m", "128M", "-smp", "3", "-nographic",
             "-global", "virtio-mmio.force-legacy=false",
             "-drive", "file=%s,if=none,format=raw,id=x0,snapshot=on" % image,
             "-device", "virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0"],
            cwd=str(ROOT), encoding="utf-8", codec_errors="replace",
            timeout=15, echo=False,
        )
        self.child.logfile_read = self.log
        self.child.expect_exact("$ ")

    def command(self, text):
        self.child.send(text + "\n")
        self.child.expect_exact("$ ")
        return self.child.before.replace("\r", "")

    def close(self):
        if self.child.isalive():
            self.child.send("\x01x")
            self.child.expect(pexpect.EOF)
        self.child.close()
        self.log.close()


class ShellTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        subprocess.run(["make", "-s", "kernel/kernel", "fs.img"], cwd=ROOT, check=True)
        cls.tmp = tempfile.TemporaryDirectory(prefix="xv6-shell-")
        cls.addClassCleanup(cls.tmp.cleanup)
        directory = Path(cls.tmp.name)
        # Ask make for the active toolchain, preserving TOOLPREFIX overrides.
        cc = subprocess.check_output([
            "make", "-s", "--no-print-directory", "--eval",
            "m1-toolchain: ; @echo $(CC)", "m1-toolchain"
        ], cwd=ROOT, text=True).strip()
        obj = directory / "probe.o"
        binary = directory / "_consoleprobe"
        subprocess.run([
            cc, "-Wall", "-Werror", "-O", "-march=rv64gc", "-mcmodel=medany",
            "-ffreestanding", "-fno-builtin", "-fno-stack-protector", "-fno-pie",
            "-I.", "-c", "tests/consoleprobe.c", "-o", str(obj)
        ], cwd=ROOT, check=True)
        subprocess.run([
            cc, "-nostdlib", "-no-pie", "-Wl,--build-id=none",
            "-Wl,-z,max-page-size=4096", "-T", "user/user.ld", "-o", str(binary),
            str(obj), "user/ulib.o", "user/usys.o", "user/printf.o", "user/umalloc.o"
        ], cwd=ROOT, check=True)
        cls.image = directory / "fs.img"
        inputs = [ROOT / "README", ROOT / "user/findtest.sh", ROOT / "user/sixfive.txt"]
        inputs += sorted(p for p in (ROOT / "user").glob("_*")
                         if p.is_file() and not p.suffix)
        for path in inputs:
            (directory / path.name).symlink_to(path)
        (directory / "nonl.sh").write_text("echo NO_NEWLINE")
        (directory / "longline.sh").write_text("echo " + "x" * 140 + "\necho AFTER_LONG\n")
        subprocess.run([
            str(ROOT / "mkfs/mkfs"), "fs.img", *[p.name for p in inputs], binary.name,
            "nonl.sh", "longline.sh"
        ], cwd=directory, check=True)

    def setUp(self):
        self.console = Console(self._testMethodName, self.image)
        self.addCleanup(self.console.close)

    def test_script_has_no_prompt(self):
        c = self.console
        c.command("echo echo SCRIPT_OK > m1script")
        c.child.send("sh < m1script\n")
        c.child.expect_exact("SCRIPT_OK\r\n")
        # Extra script prompts would occur either before the result or at EOF.
        self.assertNotIn("$ ", c.child.before)
        c.child.expect_exact("$ ")
        self.assertEqual(c.child.before.replace("\r", ""), "")
        self.assertIn("AFTER", c.command("echo AFTER"))

    def test_tab_command_and_file_before_enter(self):
        c = self.console
        c.child.send("ec\t")
        c.child.expect_exact("echo")
        c.child.send(" TAB_OK\n")
        c.child.expect_exact("\r\nTAB_OK\r\n")
        c.child.expect_exact("$ ")
        c.child.send("cat REA\t")
        c.child.expect_exact("README")
        c.child.send("\n")
        c.child.expect_exact("xv6 is a re-implementation")
        c.child.expect_exact("$ ")

    def test_tab_multiple_and_no_match(self):
        c = self.console
        c.child.send("gr\t")
        c.child.expect_exact("\r\n")
        c.child.expect_exact("$ gr")
        listed = c.child.before
        self.assertIn("grep", listed)
        self.assertIn("grind", listed)
        c.child.send("\x15")  # Ctrl-U clears the unfinished command.
        c.child.send("echo NOMATCH_\tOK\n")
        c.child.expect_exact("\r\nNOMATCH_OK\r\n")
        c.child.expect_exact("$ ")

    def test_history_and_expansion(self):
        c = self.console
        c.command("echo first")
        out = c.command("!!")
        self.assertIn("\necho first\nfirst\n", out)
        c.command("echo second")
        out = c.command("!1")
        self.assertIn("\necho first\nfirst\n", out)
        out = c.command("history")
        self.assertIn("1 echo first\n", out)
        self.assertIn("5 history\n", out)

    def test_arrows_restore_draft_and_allow_editing(self):
        c = self.console
        c.command("echo ARROW")
        c.child.send("echo DRAFT")
        c.child.expect_exact("echo DRAFT")
        c.child.send("\x1b[A")
        c.child.expect_exact("echo ARROW")
        c.child.send("\x1b[B")
        c.child.expect_exact("echo DRAFT")
        c.child.send("\x1b[A")
        c.child.expect_exact("echo ARROW")
        c.child.send("\x7fS\n")
        c.child.expect_exact("\r\nARROS\r\n")
        c.child.expect_exact("$ ")

    def test_history_errors_and_eviction(self):
        c = self.console
        self.assertIn("history: no previous command", c.command("!!"))
        self.assertIn("history: event not found", c.command("!999"))
        for i in range(12):
            c.command("echo item%d" % i)
        out = c.command("history")
        self.assertNotIn("1 echo item0\n", out)
        self.assertIn("12 echo item11\n", out)
        self.assertIn("13 history\n", out)
        self.assertIn("history: event not found", c.command("!1"))

    def test_normal_program_console_and_pipes(self):
        c = self.console
        self.assertIn("PIPE_OK", c.command("echo PIPE_OK | cat"))
        c.child.send("cat\n")
        c.child.expect_exact("cat\r\n")
        c.child.send("COOKED\x7fX\n")
        c.child.expect_exact("\r\nCOOKEX\r\n")
        c.child.send("\x04")
        c.child.expect_exact("$ ")
        self.assertIn("DONE", c.command("echo DONE"))

    def test_typeahead_backspace_reaches_program_as_cooked_input(self):
        c = self.console
        c.child.send("wc\nFOO\x7fX\n\x04")
        c.child.expect_exact("$ ")
        self.assertRegex(c.child.before.replace("\r", ""), r"(?m)(?:^|\x04)1 1 4 $")

    def test_typeahead_ctrl_u_reaches_program_as_cooked_input(self):
        c = self.console
        c.child.send("wc\nOLD\x15NEW\n\x04")
        c.child.expect_exact("$ ")
        self.assertRegex(c.child.before.replace("\r", ""), r"(?m)(?:^|\x04)1 1 4 $")

    def test_background_exit_does_not_flush_cooked_input(self):
        c = self.console
        c.command("consoleprobe delayed &")
        c.child.send("cat\n")
        c.child.expect_exact("cat\r\n")
        c.child.send("PARTIAL")
        c.child.expect_exact("PARTIAL")  # Kernel's normal keyboard echo.
        with self.assertRaises(pexpect.TIMEOUT):
            c.child.expect_exact("PARTIAL", timeout=2)
        # A background exit must not commit the unfinished line: backspace
        # still needs to erase its last character inside the cooked buffer.
        c.child.send("\x7fX\n")
        c.child.expect_exact("\r\nPARTIAX\r\n")
        c.child.send("\x04")
        c.child.expect_exact("$ ")

    def test_raw_mode_lifecycle_and_ownership(self):
        c = self.console
        for mode, expected in [("invalid", "INVALID_OK"), ("owner", "OWNER_OK"),
                               ("exit", "RAW_EXIT"), ("kill", "KILL_OK")]:
            self.assertIn(expected, c.command("consoleprobe " + mode))
        c.child.send("consoleprobe exec\n")
        c.child.expect_exact("consoleprobe exec\r\n")
        c.child.send("EXEC_OK\n")
        c.child.expect_exact("EXEC_OK\r\nEXEC_OK\r\n")
        c.child.send("\x04")
        c.child.expect_exact("$ ")

    def test_completion_in_current_directory_and_long_name(self):
        c = self.console
        c.command("mkdir sub")
        c.command("echo CONTENT > sub/abcdefghijklmn")
        c.command("cd sub")
        c.child.send("/cat abc\t")
        c.child.expect_exact("abcdefghijklmn")  # DIRSIZ bytes, no on-disk NUL.
        c.child.send("\n")
        c.child.expect_exact("\r\nCONTENT\r\n")
        c.child.expect_exact("$ ")
        c.command("cd /")
        c.child.send("cat sub/abc\t")
        c.child.expect_exact("abcdefghijklmn")
        c.child.send("\n")
        c.child.expect_exact("\r\nCONTENT\r\n")
        c.child.expect_exact("$ ")

    def test_script_eof_and_history_expansion_with_leading_spaces(self):
        c = self.console
        self.assertIn("NO_NEWLINE\n", c.command("sh < nonl.sh"))
        out = c.command("sh < longline.sh")
        self.assertIn("sh: line too long\n", out)
        self.assertIn("AFTER_LONG\n", out)
        self.assertIn("PIPE_SCRIPT", c.command("echo echo PIPE_SCRIPT | sh"))
        text = "x" * 110
        c.command("echo " + text)
        out = c.command("          !!")
        self.assertIn("\necho " + text + "\n" + text + "\n", out)

    def test_tab_empty_prefix_skips_devices(self):
        c = self.console
        c.child.send("\t")
        c.child.expect_exact("$ ", timeout=3)
        self.assertIn("echo", c.child.before)
        self.assertNotIn("console", c.child.before.split())
        c.child.send("co\t")
        c.child.send("\x15echo DEVICE_SAFE\n")
        c.child.expect_exact("\r\nDEVICE_SAFE\r\n")
        c.child.expect_exact("$ ")


if __name__ == "__main__":
    unittest.main(verbosity=2)
