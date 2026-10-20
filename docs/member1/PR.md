Implement bounded memdump and interactive shell improvements

Memdump currently has an empty implementation, and the shell always prints prompts and lacks completion/history.
This implements all six memdump formats with length checks, suppresses script prompts, and adds interactive Tab
completion plus history, !!, !n, and Up/Down navigation.

Console input needs to reach the shell before Enter. Add a PID-owned consolemode syscall (23 on util), enable it
only while reading interactive commands, and restore cooked mode before command execution and on owner exit/kill/exec.
Kernel/syscall changes were explicitly approved by the user, who will coordinate with the team leader.

Validation:

- 15 hand-checked Memdump cases, including guard-page bounds tests: pass.
- 12 real QEMU Shell/console tests, with Tab/arrows sent before Enter: pass.
- ./grade-lab-util memdump: all 3 groups OK.
- Synced util at 1c1c51e, including Member 2's Find/Exec. Full grade-lab-util: 95/156, 10 groups pass, 9 fail.
  All seven Find/Exec groups pass. Remaining failures are missing sleep, sixfive, and time.txt, outside this scope.
  The old script monitor prints a timeout warning, but the script test is ultimately graded OK.
- Independent review identified and verified fixes for device completion and cooked-input cleanup.
- All C/header files pass clang-format 23.1.3; git diff --check passes. Remove only surplus blank lines in
  kernel/param.h and kernel/riscv.h to unblock the existing CI format step; C tokens are unchanged.
  Ubuntu 24's clang-format 18 cannot read ReflowComments: Never, so local verification used a newer formatter.

No Makefile or grading-script changes. Includes a concise report, demo/viva guide, test evidence and Member 1's
code-only util.patch. The team should regenerate the final util.patch after integrating all members.

Limits: 127-character input line, session-local history, original relative executable lookup, and valid target pointers
assumed for memdump's s format.
