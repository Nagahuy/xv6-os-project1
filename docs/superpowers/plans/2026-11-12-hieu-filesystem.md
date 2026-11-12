# Hieu filesystem implementation plan

> Execute inline with superpowers:executing-plans; test each deliverable before committing.

**Goal:** Complete Plan_DoAn2.md sections 4.2.1 and 4.2.2 for Trung Hieu.
**Spec:** C:/Users/VuiTrinhThiKim/Downloads/Plan_DoAn2.md, plus the user's correction authorizing a branch from current code instead of origin/fs.
**Base:** 43088aa420ae619d50f0595c910650659bef4874, existing util code.
**Branch:** trung-hieu/largefile-symlink. Do not push to util or create shared submission branches.
**Architecture:** Keep 13 inode addresses with 11 direct, one single and one double indirect; bmap allocates lazily, itrunc frees both levels. Add symlink syscall 24 (23 already consolemode), T_SYMLINK=4 and O_NOFOLLOW=0x800. Store NUL-terminated target data before publishing the new directory entry; open follows at most 10 links, relative targets use cwd as in lab scope. Preserve link/unlink semantics.
**Tech stack:** C, xv6 RV64, Ubuntu WSL, GCC RISC-V, QEMU, Python pexpect.

## Global constraints
- Keep disk inode 64 bytes / 13 addresses; BSIZE=1024, FSSIZE=200000 for LAB=fs.
- Do not modify existing graders or weaken tests. No disk images, generated files or test logs in code commits.
- Preserve prior Shell/console and other members' work. Preserve user's untracked report/.vscode.
- Author new test programs because this base has neither bigfile nor symlinktest; label as project tests, not official teacher tests.
- Separate commits for largefile and symlink; final report lists tested SHAs and limits.

## Review focus
- Inode address count / on-disk image compatibility; use a fresh image.
- Direct/single/double boundaries and second inner table; bigfile verifies all blocks by index.
- Truncation leaks; compare free bitmap counts before/after full-file creation, truncate and unlink.
- Symlink chains, cycles, missing targets, flags, corrupt payloads; behavioral edge tests.
- Concurrent creation/open/removal and reference cleanup; independent processes, full usertests.

### Task 1: Large files
Files: kernel/fs.h, kernel/file.h, kernel/fs.c, conf/lab.mk, Makefile; user/bigfile.c; tests/run_project2.py.
Consumes: bmap(struct inode*, uint), itrunc(struct inode*), read/write API.
Produces: read/write up to 65803 blocks and full reclamation, selected LAB=fs build.
- [ ] Write bigfile test that writes 65803 indexed blocks, rejects another block, verifies all data; runner uses QEMU and inspects image bitmap after truncate/unlink/reboot.
- [ ] Run against base, record expected failure at 268 blocks (old MAXFILE).
- [ ] Set NDIRECT=11, MAXFILE=11+256+256*256, addrs[NDIRECT+2] in both inode forms. Implement double-indirect bmap/itrunc with balanced buffers and journaling.
- [ ] Ensure mkfs does not index outside its single-indirect array; support double-indirect image input too if adding general image tests. Build fresh LAB=fs image.
- [ ] Run bigfile, boundary/reclamation checks and usertests -q. Commit fs: support doubly indirect file blocks.

### Task 2: Symlink
Files: kernel/stat.h, fcntl.h, syscall.h, syscall.c, sysfile.c; user/user.h, usys.pl, symlinktest.c; Makefile.
Consumes: existing create/namei/writei/readi/inode locking and syscall routing.
Produces: int symlink(const char *target, const char *path); open following / NOFOLLOW.
- [ ] Write behavior tests and temporary raw ecall probe to show missing syscall against Task 1.
- [ ] Implement symlink API and creation before directory publication; follow links in open, reject malformed / missing / cycles, preserve O_CREATE/O_TRUNC behavior and nofollow.
- [ ] Test dangling target creation, normal link/chain, 10/11-depth boundary, self/cross cycle, nofollow reads, hardlink/unlink, concurrent calls, fd exhaustion and invalid arguments.
- [ ] Run bigfile, symlinktest and usertests -q on the combined branch. Commit fs: add symbolic links and nofollow open.

### Task 3: Handoff
- [ ] Review complete diff; fresh reviewer if available under executing-plans skill. Fix material findings with reproducer tests.
- [ ] Markdown report with short code excerpts, files/reasons/test results/limits, baseline exception and current PR target availability.
- [ ] Push only personal branch. Open PR to submit/fs if it exists; otherwise report missing target without creating leader-owned branch.

## Execution ledger
Ruling: User explicitly authorized current code as base; no origin/fs prerequisite. Work in place on named feature branch, preserving untracked local files.
Ruling: Design is already specified in the supplied plan and accepted in teaching exchange; proceed without repeating design/approval questions.
Pre-flight: Task 2 uses Task 1 inode layout, both use LAB=fs; tests must run on fresh images. Existing util syscall 23 requires symlink=24.
