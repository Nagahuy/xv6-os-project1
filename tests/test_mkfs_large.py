#!/usr/bin/env python3
"""Catch bad double-indirect mkfs indices and a bitmap limited to 8192 blocks."""
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    with tempfile.TemporaryDirectory(prefix="xv6-mkfs-large-") as tmp:
        folder = Path(tmp)
        payload = folder / "payload"
        with payload.open("wb") as out:
            for bn in range(9000):
                out.write(struct.pack("<I", bn) + bytes([bn % 251]) * 1020)
        binary = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else ROOT / "mkfs/mkfs"
        subprocess.run([str(binary), "disk.img", "payload"],
                       cwd=folder, check=True, stdout=subprocess.DEVNULL)
        with (folder / "disk.img").open("rb") as disk:
            def block(number):
                disk.seek(number * 1024)
                return disk.read(1024)

            sb = struct.unpack("<8I", block(1)[:32])
            assert sb[1] == 200000
            inode = block(sb[6])[128:192]  # inode 2: first input file
            assert len(inode) == 64
            assert struct.unpack_from("<I", inode, 8)[0] == 9216000
            addrs = struct.unpack_from("<13I", inode, 12)
            single = struct.unpack("<256I", block(addrs[11]))
            outer = struct.unpack("<256I", block(addrs[12]))
            seen = set()
            for bn in range(9000):
                if bn < 11:
                    address = addrs[bn]
                elif bn < 267:
                    address = single[bn - 11]
                else:
                    index = bn - 267
                    inner = struct.unpack("<256I", block(outer[index // 256]))
                    address = inner[index % 256]
                assert address not in seen and address != 0, bn
                seen.add(address)
                content = block(address)
                assert content == struct.pack("<I", bn) + bytes([bn % 251]) * 1020, bn
            metadata = {addrs[11], addrs[12]} | {n for n in outer if n}
            assert len(metadata) == 37  # 1 single + 1 root + 35 inner tables
            assert not (metadata & seen)
            allocated = set(range(sb[7] + 25 + 1)) | metadata | seen
            # mkfs allocates contiguously, including the root directory block.
            used = max(allocated) + 1
            assert len(allocated) == used
            disk.seek(sb[7] * 1024)
            bitmap = disk.read(25000)
            assert sum(b.bit_count() for b in bitmap) == used
            assert all(bitmap[i // 8] & (1 << (i % 8)) for i in range(used))
        print("PASS mkfs: 9000 blocks, all data and index addresses, multiple bitmap blocks")


if __name__ == "__main__":
    main()
