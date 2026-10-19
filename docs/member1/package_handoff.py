#!/usr/bin/env python3
"""Export and verify the Member 1 code patch and retain concrete test evidence."""

from pathlib import Path
import io
import json
import shutil
import subprocess
import tarfile
import tempfile
import zipfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
RESULTS = ROOT / "test-results"
release = ROOT / "Release"
release.mkdir(exist_ok=True)
evidence = HERE / "evidence"
evidence.mkdir(exist_ok=True)

baseline = json.loads((RESULTS / "baseline-grades.json").read_text())
final = json.loads((RESULTS / "final-grades.json").read_text())
if baseline != final:
    raise SystemExit("Grader results changed; inspect before packaging.")
print("Grader comparison: identical; passed=%d failed=%d" % (
    sum(v > 0 for v in final.values()), sum(v == 0 for v in final.values())))
for name in ["memdump-guard.log", "shell-after.log"]:
    if not (RESULTS / name).read_text(encoding="utf-8").rstrip().endswith("\nOK"):
        raise SystemExit("Member 1 tests are not green: " + name)
if (RESULTS / "memdump-grade.log").read_text(encoding="utf-8").count(": OK") != 3:
    raise SystemExit("Expected all three official Memdump test groups to pass.")

for name in [
    "baseline-grades.json", "final-grades.json", "memdump-guard.log", "shell-after.log",
    "memdump-grade.log", "grade-before-shell.log", "grade-final.log", "shell-before.log", "format-check.log",
    "console-before-fix.log", "device-before-fix.log",
    "test_tab_command_and_file_before_enter.log", "test_tab_multiple_and_no_match.log",
    "test_tab_empty_prefix_skips_devices.log", "test_history_and_expansion.log",
    "test_arrows_restore_draft_and_allow_editing.log", "test_script_has_no_prompt.log",
    "test_raw_mode_lifecycle_and_ownership.log",
    "test_background_exit_does_not_flush_cooked_input.log",
]:
    shutil.copyfile(RESULTS / name, evidence / name)

paths = subprocess.check_output([
    "git", "diff", "--name-only", "origin/util", "--", "kernel", "user"
], cwd=ROOT, text=True).splitlines()
patch = release / "util.patch"
with patch.open("wb") as stream:
    subprocess.run([
        "git", "diff", "--binary", "origin/util", "--", "kernel", "user"
    ], cwd=ROOT, stdout=stream, check=True)
if not paths:
    raise SystemExit("Expected source changes in util.patch.")

with tempfile.TemporaryDirectory(prefix="xv6-patch-check-") as folder:
    original = subprocess.check_output(["git", "archive", "origin/util"], cwd=ROOT)
    with tarfile.open(fileobj=io.BytesIO(original)) as archive:
        archive.extractall(folder, filter="data")
    git_apply = ["git", "-c", "core.autocrlf=false", "-c", "core.filemode=false", "apply"]
    subprocess.run([*git_apply, "--check", str(patch)], cwd=folder, check=True)
    subprocess.run([*git_apply, str(patch)], cwd=folder, check=True)
    for name in paths:
        if (Path(folder) / name).read_bytes() != (ROOT / name).read_bytes():
            raise SystemExit("Patched source differs: " + name)
print("Patch verified against clean origin/util: %d source files" % len(paths))

bundle = release / "member1-handoff.zip"
with zipfile.ZipFile(bundle, "w", zipfile.ZIP_DEFLATED) as archive:
    archive.write(patch, "Release/util.patch")
    archive.write(HERE / "Report_Member1.pdf", "Report_Member1.pdf")
    for path in sorted(HERE.glob("*.md")):
        archive.write(path, "docs/member1/" + path.name)
    for path in sorted(evidence.iterdir()):
        archive.write(path, "evidence/" + path.name)
    for path in sorted((ROOT / "tests").iterdir()):
        if path.is_file():
            archive.write(path, "tests/" + path.name)
print(bundle)
