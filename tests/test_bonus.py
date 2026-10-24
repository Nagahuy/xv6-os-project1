#!/usr/bin/env python3
"""Run with /usr/bin/python3 tests/test_bonus.py; requires python3-pexpect."""

from pathlib import Path
import subprocess
import unittest

import pexpect

ROOT = Path(__file__).resolve().parents[1]


class BonusTests(unittest.TestCase):
    def test_init_protection_and_normal_kill(self):
        subprocess.run(["make", "-s", "kernel/kernel", "fs.img"], cwd=ROOT, check=True)
        child = pexpect.spawn(
            "qemu-system-riscv64",
            ["-machine", "virt", "-bios", "none", "-kernel", "kernel/kernel",
             "-m", "128M", "-smp", "3", "-nographic",
             "-global", "virtio-mmio.force-legacy=false",
             "-drive", "file=fs.img,if=none,format=raw,id=x0,snapshot=on",
             "-device", "virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0"],
            cwd=str(ROOT), encoding="utf-8", timeout=30, echo=False,
        )
        try:
            child.expect_exact("$ ")
            child.sendline("exploit")
            outcome = child.expect_exact([
                "exploit: result=-1\r\n", "panic: init exiting",
            ])
            self.assertEqual(outcome, 0, "kill(1) caused kernel panic")
            child.expect_exact("$ ")
            child.sendline("echo ALIVE")
            child.expect_exact("\r\nALIVE\r\n")
            child.expect_exact("$ ")
            for name in ("killstatus", "preempt"):
                child.sendline("usertests " + name)
                outcome = child.expect_exact([
                    "ALL TESTS PASSED", "SOME TESTS FAILED", "panic:",
                ])
                self.assertEqual(outcome, 0, name + " failed")
                child.expect_exact("$ ")
        finally:
            if child.isalive():
                child.send("\x01x")
                child.expect(pexpect.EOF)
            child.close()


if __name__ == "__main__":
    unittest.main(verbosity=2)
