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
- make grade: 25/156, with exactly the same test-status/score map as before Shell changes. Missing sleep, sixfive,
  find/exec, and time.txt remain outside this member's scope. The script monitor can also wait to timeout after DONE
  when prompts are correctly suppressed; the transcript and explanation are in docs/member1/TEST_RESULTS.md.
- Independent review identified and verified fixes for device completion and cooked-input cleanup.

No Makefile or grading-script changes. Includes a concise report, demo/viva guide, test evidence and Member 1's
code-only util.patch. The team should regenerate the final util.patch after integrating all members.

Limits: 127-character input line, session-local history, original relative executable lookup, and valid target pointers
assumed for memdump's s format.
