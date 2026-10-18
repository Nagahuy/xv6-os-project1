#!/usr/bin/env python3
"""Exercise the real memdump() against hand-checked data and guard pages.

Run on Linux: python3 tests/test_memdump.py
The harness links user/memdump.c to libc; QEMU/grader tests are separate.
"""

import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

HARNESS = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

void memdump(char *, char *, int);

int main(int argc, char **argv) {
  if (argc != 2) return 2;
  long page = sysconf(_SC_PAGESIZE);
  char *area = mmap(0, 2 * page, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (area == MAP_FAILED || mprotect(area + page, page, PROT_NONE)) return 3;
  char *end = area + page;
  int which = atoi(argv[1]);
  switch (which) {
  case 0: {
    unsigned char bytes[] = {42, 0, 0, 0, 'A', 'H', 'i', 0};
    memcpy(end - 8, bytes, 8);
    memdump("icS", end - 8, 8);
    break;
  }
  case 1:
    memcpy(end - 8, "deadc0de", 8);
    memdump("hhcccc", end - 8, 8);
    break;
  case 2:
    memcpy(end - 8, "deadc0de", 8);
    memdump("p", end - 8, 8);
    break;
  case 3:
    memcpy(end - 3, "abc", 3);
    memdump("S", end - 3, 3);
    break;
  case 4: {
    char *text = "pointer string";
    memcpy(end - 8, &text, 8);
    memdump("s", end - 8, 8);
    break;
  }
  case 5: {
    unsigned char bytes[] = {'A', 255, 255, 255, 255};
    memcpy(end - 5, bytes, 5);
    memdump("ci", end - 5, 5);
    break;
  }
  case 6: memdump("i", end - 3, 3); break;
  case 7: memdump("p", end - 7, 7); break;
  case 8: memdump("h", end - 1, 1); break;
  case 9: memdump("c", end, 0); break;
  case 10: memdump("s", end - 7, 7); break;
  case 11:
    memcpy(end - 4, "abc\n", 4);
    memdump("iic", end - 4, 4);
    break;
  case 12:
    memcpy(end - 4, "a\0bc", 4);
    memdump("S", end - 4, 4);
    break;
  case 13: memdump("S", end, 0); break;
  case 14: {
    unsigned char bytes[] = {0, 128};
    memcpy(end - 2, bytes, 2);
    memdump("h", end - 2, 2);
    break;
  }
  default: return 4;
  }
  munmap(area, 2 * page);
  return 0;
}
'''


class MemdumpTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory(prefix="xv6-memdump-")
        folder = Path(cls.tmp.name)
        harness = folder / "harness.c"
        harness.write_text(HARNESS)
        cc = os.environ.get("CC", "cc")
        subprocess.run([
            cc, "-fno-builtin", "-Dmain=xv6_main", "-I", str(ROOT),
            "-c", str(ROOT / "user/memdump.c"), "-o", str(folder / "memdump.o")
        ], check=True)
        cls.binary = folder / "test-memdump"
        subprocess.run([
            cc, str(harness), str(folder / "memdump.o"), "-o", str(cls.binary)
        ], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_formats_and_bounds(self):
        # Replacing bounds checks with a padded read, or reading S via %s,
        # must fail: the first byte beyond the input is an inaccessible page.
        cases = [
            (0, "42\nA\nHi\n"),
            (1, "25956\n25697\nc\n0\nd\ne\n"),
            (2, "6564306364616564\n"),
            (3, "abc\n"),
            (4, "pointer string\n"),
            (5, "A\n-1\n"),
            (6, "memdump: not enough data for 'i'\n"),
            (7, "memdump: not enough data for 'p'\n"),
            (8, "memdump: not enough data for 'h'\n"),
            (9, "memdump: not enough data for 'c'\n"),
            (10, "memdump: not enough data for 's'\n"),
            (11, "174285409\nmemdump: not enough data for 'i'\n"),
            (12, "a\n"),
            (13, "\n"),
            (14, "-32768\n"),
        ]
        for case, expected in cases:
            with self.subTest(case=case):
                result = subprocess.run(
                    [str(self.binary), str(case)], capture_output=True, text=True
                )
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(result.stdout, expected)


if __name__ == "__main__":
    unittest.main(verbosity=2)
