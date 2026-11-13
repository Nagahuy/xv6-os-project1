#!/usr/bin/env python3
"""Run real QEMU FS regressions; keep transcripts in ignored test-results/.

Linux/WSL: python3 tests/run_project2.py --suite largefile
The disk bitmap checks catch index/data leaks that a second bigfile alone misses.
"""
import argparse
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile

import pexpect

ROOT = Path(__file__).resolve().parents[1]
RESULTS = ROOT / "test-results" / "project2"


class Console:
    def __init__(self, image, name, cpus=1):
        self.log = (RESULTS / (name + ".log")).open("w", encoding="utf-8")
        self.child = pexpect.spawn("qemu-system-riscv64", [
            "-machine", "virt", "-bios", "none", "-kernel", "kernel/kernel",
            "-m", "128M", "-smp", str(cpus), "-nographic",
            "-global", "virtio-mmio.force-legacy=false",
            "-drive", f"file={image},if=none,format=raw,id=x0,cache=unsafe",
            "-device", "virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0",
        ], cwd=ROOT, encoding="utf-8", timeout=1800, echo=False)
        self.child.logfile_read = self.log
        self.child.expect_exact("$ ")

    def command(self, command):
        self.child.sendline(command)
        self.child.expect_exact("$ ")
        output = self.child.before.replace("\r", "")
        if "panic:" in output or "FAIL" in output:
            raise AssertionError(output[-2000:])
        return output

    def close(self):
        if self.child.isalive():
            self.child.send("\x01x")
            self.child.expect(pexpect.EOF, timeout=20)
        self.child.close()
        self.log.close()


def allocated(image):
    with open(image, "rb") as disk:
        disk.seek(1024)
        magic, size, _, _, _, _, _, bitmap = struct.unpack("<8I", disk.read(32))
        assert magic == 0x10203040
        disk.seek(bitmap * 1024)
        bits = disk.read((size + 7) // 8)
        return sum(byte.bit_count() for byte in bits)


def boot_commands(image, name, commands, cpus=1):
    c = Console(image, name, cpus)
    try:
        outputs = []
        for command, marker in commands:
            output = c.command(command)
            assert marker in output, (command, output[-2000:])
            outputs.append(output)
        return outputs
    finally:
        c.close()


def largefile(image):
    before = allocated(image)
    boot_commands(image, "largefile-keep", [("bigfile keep", "done; ok")])
    used = allocated(image) - before
    # 65803 data + 1 single index + 1 double root + 256 inner indices.
    assert used == 66061, ("allocated blocks", used)
    full = image.with_name("full.img")
    shutil.copyfile(image, full)
    boot_commands(image, "largefile-reboot-truncate", [
        ("bigfile verify", "done; ok"), ("bigfile truncate", "done; ok")])
    assert allocated(image) == before, "truncate leaked data/index blocks"
    # Test unlink on the same full-size content without repeating a long
    # allocation workload; then create another double-indirect file.
    boot_commands(full, "largefile-unlink", [("bigfile unlink", "done; ok")])
    assert allocated(full) == before, "full-size unlink leaked blocks"
    boot_commands(image, "largefile-reuse-unlink", [("bigfile small", "done; ok")])
    assert allocated(image) == before, "unlink leaked data/index blocks"
    print("PASS largefile: 65803 indexed blocks, reboot, truncate, reuse, unlink; bitmap restored", flush=True)


def fragmented():
    """Synthetic disposable full disk: target data touches all 25 bitmap pages."""
    with tempfile.TemporaryDirectory(prefix="xv6-fragmented-") as tmp:
        folder = Path(tmp)
        # Build a test-only executable using the same flags/linker as user tools.
        cc = subprocess.check_output(["make", "-s", "--eval",
            "p2-cc: ; @echo $(CC)", "p2-cc"], cwd=ROOT, text=True).strip()
        subprocess.run([cc, "-Wall", "-Werror", "-O", "-march=rv64gc", "-mcmodel=medany",
            "-ffreestanding", "-fno-builtin", "-fno-pie", "-fno-stack-protector", "-I.",
            "-c", "tests/fsedge.c", "-o", str(folder / "edge.o")], cwd=ROOT, check=True)
        subprocess.run([cc, "-nostdlib", "-no-pie", "-Wl,--build-id=none", "-Wl,-z,max-page-size=4096",
            "-T", "user/user.ld", "-o", str(folder / "_fsedge"), str(folder / "edge.o"),
            "user/ulib.o", "user/usys.o", "user/printf.o", "user/umalloc.o"], cwd=ROOT, check=True)
        names = ["_init", "_sh"]
        for name in names:
            (folder / name).symlink_to(ROOT / "user" / name)
        (folder / "frag.data").write_bytes(b"")
        for name in ["frag.w0", "frag.w1"]:
            (folder / name).write_bytes(bytes(3072))
        image = folder / "disk.img"
        subprocess.run([str(ROOT / "mkfs/mkfs"), str(image), *names,
            "_fsedge", "frag.data", "frag.w0", "frag.w1"], cwd=folder,
            stdout=subprocess.DEVNULL, check=True)
        with image.open("r+b") as disk:
            disk.seek(1024)
            sb = struct.unpack("<8I", disk.read(32))
            # Inode 5: init=2, sh=3, fsedge=4, frag.data=5.
            disk.seek(sb[6] * 1024 + 5 * 64)
            inode = bytearray(disk.read(64))
            addresses = [region * 8192 + 4096 for region in range(24)] + [199000]
            index = 4097
            struct.pack_into("<I", inode, 8, 25 * 1024)
            struct.pack_into("<13I", inode, 12, *addresses[:11], index, 0)
            disk.seek(sb[6] * 1024 + 5 * 64)
            disk.write(inode)
            disk.seek(index * 1024)
            disk.write(struct.pack("<256I", *addresses[11:], *([0] * 242)))
            # Pretend every remaining block belongs to filler files; no free
            # blocks exist. All modifications stay in this temporary image.
            disk.seek(sb[7] * 1024)
            disk.write(bytes([255]) * (25 * 1024))
        before = allocated(image)
        boot_commands(image, "fragmented-enospc", [("fsedge", "fsedge: all tests passed")], cpus=3)
        assert allocated(image) == before - 26, "fragmented truncate leaked blocks"
        print("PASS fragmented truncate across 25 bitmap pages with concurrent writers, ENOSPC rollback", flush=True)
        (folder / "partial").write_bytes(bytes(267 * 1024))
        for free in (0, 1, 2):
            subprocess.run([str(ROOT / "mkfs/mkfs"), str(image), *names,
                "_fsedge", "partial"], cwd=folder, stdout=subprocess.DEVNULL, check=True)
            with image.open("r+b") as disk:
                disk.seek(1024)
                sb = struct.unpack("<8I", disk.read(32))
                disk.seek(sb[7] * 1024)
                bitmap = bytearray([255]) * (25 * 1024)
                for block in range(199980, 199980 + free):
                    bitmap[block // 8] &= ~(1 << (block % 8))
                disk.write(bitmap)
            before = allocated(image)
            boot_commands(image, f"double-enospc-{free}", [("fsedge partial", "partial case passed")])
            assert allocated(image) == before + free, "unexpected partial index allocation"
            boot_commands(image, f"double-reclaim-{free}", [("fsedge reclaim", "partial case passed")])
            assert allocated(image) == before - 268, "partial double-indirect allocation leaked"
        print("PASS double-indirect ENOSPC with 0/1/2 free blocks; partial indices reclaimed", flush=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--suite", choices=["largefile", "symlink", "fragmented", "usertests", "all"], default="all")
    args = parser.parse_args()
    RESULTS.mkdir(parents=True, exist_ok=True)
    with (RESULTS / "build.log").open("w") as log:
        subprocess.run(["make", "-j4", "LAB=fs", "kernel/kernel", "fs.img"],
                       cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True)
    with tempfile.TemporaryDirectory(prefix="xv6-project2-") as tmp:
        image = Path(tmp) / "fs.img"
        shutil.copyfile(ROOT / "fs.img", image)
        if args.suite in ("largefile", "all"):
            largefile(image)
        if args.suite in ("symlink", "all"):
            boot_commands(image, "symlink", [("symlinktest", "symlinktest: all tests passed")], cpus=3)
            print("PASS symlinktest", flush=True)
        if args.suite in ("usertests", "all"):
            boot_commands(image, "usertests", [("usertests -q", "ALL TESTS PASSED")])
            print("PASS usertests -q", flush=True)
        if args.suite in ("fragmented", "all"):
            fragmented()
        if args.suite == "all":
            subprocess.run(["python3", "tests/test_mkfs_large.py"], cwd=ROOT, check=True)


if __name__ == "__main__":
    main()
