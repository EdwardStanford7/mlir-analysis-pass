#!/usr/bin/env python3
"""Build and run dynamic sign checks, keeping artifacts for investigation."""

import argparse
import csv
import os
from pathlib import Path
import resource
import shlex
import signal
import subprocess
import sys


ROOT = Path(__file__).resolve().parent.parent
# argc is 1, 2, or 3. The boundary programs select different paths using argc.
INPUTS = [[], ["one"], ["one", "two"]]


def show_output(result):
    for name in ("stdout", "stderr"):
        data = getattr(result, name, None)
        if data:
            print(data.decode(errors="replace").rstrip(), flush=True)


def execute(command, directory, label, timeout=30, env=None, stdout=None):
    """Save every command and its output, including failed build stages."""
    with (directory / "commands.txt").open("a") as log:
        log.write(shlex.join(map(str, command)) + "\n")
    result = subprocess.run(
        list(map(str, command)), cwd=ROOT, env=env,
        capture_output=True, timeout=timeout,
    )
    (stdout or directory / (label + ".stdout")).write_bytes(result.stdout)
    (directory / (label + ".stderr")).write_bytes(result.stderr)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--case", default="*", help="test filename glob without .mlir")
    parser.add_argument("--build-dir", default="build", help="plugin build directory")
    parser.add_argument("--output-dir", default="build/instrumentation-tests")
    parser.add_argument("--timeout", type=float, default=5, help="seconds per execution")
    args = parser.parse_args()
    if args.timeout <= 0:
        parser.error("--timeout must be positive")
    sources = sorted((ROOT / "tests").glob(args.case + ".mlir"))
    if not sources:
        parser.error("no test programs matched --case")

    # Assertion traps should not leave large core files behind.
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    output = Path(args.output_dir).resolve()
    output.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, BUILD_DIR=str(Path(args.build_dir).resolve()))
    rows = []

    for source in sources:
        directory = output / source.stem
        directory.mkdir(exist_ok=True)
        (directory / "commands.txt").write_text("")
        baseline_ll = directory / "baseline.ll"
        instrumented_mlir = directory / "instrumented.mlir"
        instrumented_ll = directory / "instrumented.ll"
        baseline = directory / "baseline"
        instrumented = directory / "instrumented"
        stages = [
            ("baseline-export", ["mlir-translate", "--mlir-to-llvmir", source], baseline_ll),
            ("baseline-compile", ["clang", "-O0", baseline_ll, "-o", baseline], None),
            ("annotations", [ROOT / "run.sh", "--instrument", source], instrumented_mlir),
            ("instrumented-export", ["mlir-translate", "--mlir-to-llvmir", instrumented_mlir], instrumented_ll),
            ("instrumented-compile", ["clang", "-O0", instrumented_ll, "-o", instrumented], None),
        ]
        stage = "setup"
        try:
            for stage, command, stdout in stages:
                result = execute(command, directory, stage, env=env, stdout=stdout)
                if result.returncode:
                    show_output(result)
                    raise RuntimeError(f"{stage} exited {result.returncode}")
                if stage == "annotations" and "llvm.intr.trap" not in instrumented_mlir.read_text():
                    raise RuntimeError("instrumentation inserted no checks")
        except (OSError, subprocess.TimeoutExpired, RuntimeError) as error:
            print(f"ERROR {source.stem}: {error}", flush=True)
            if isinstance(error, subprocess.TimeoutExpired):
                show_output(error)
            rows.append((source.stem, "-", "ERROR", str(error)))
            continue

        for index, inputs in enumerate(INPUTS):
            label = f"args-{index}"
            failure_output = None
            try:
                original = execute([baseline, *inputs], directory, label + "-baseline", args.timeout)
                if original.returncode != 0:
                    status, detail = "BASELINE_ERROR", f"baseline exited {original.returncode}"
                    failure_output = original
                else:
                    checked = execute([instrumented, *inputs], directory, label + "-instrumented", args.timeout)
                    if checked.returncode in (-signal.SIGILL, -signal.SIGTRAP, -signal.SIGABRT):
                        status = "TRAP"
                        detail = signal.Signals(-checked.returncode).name
                    elif checked.returncode < 0:
                        status, detail = "CRASH", signal.Signals(-checked.returncode).name
                    elif checked.returncode != original.returncode or checked.stdout != original.stdout:
                        status, detail = "MISMATCH", f"instrumented exit {checked.returncode} or stdout differs"
                    else:
                        status, detail = "PASS", "baseline and instrumented execution agree"
                    if status != "PASS":
                        failure_output = checked
                        if status == "MISMATCH":
                            detail += f"; baseline stdout={original.stdout!r}, instrumented stdout={checked.stdout!r}"
            except subprocess.TimeoutExpired as error:
                status, detail = "TIMEOUT", f"execution exceeded {args.timeout}s"
                failure_output = error
            except OSError as error:
                status, detail = "ERROR", str(error)
            rows.append((source.stem, str(index), status, detail))
            if status == "PASS":
                arguments = shlex.join(inputs) if inputs else "no arguments"
                print(f"PASS {source.stem} [{arguments}]", flush=True)
            else:
                print(f"{status} {source.name} arguments={inputs!r}: {detail}", flush=True)
                if failure_output is not None:
                    show_output(failure_output)

    with (output / "results.tsv").open("w", newline="") as summary:
        writer = csv.writer(summary, delimiter="\t")
        writer.writerow(("program", "extra_arguments", "status", "detail"))
        writer.writerows(rows)
    passed = sum(row[2] == "PASS" for row in rows)
    # Continue through every program/input, but make failures visible to make/CI.
    return 0 if passed == len(rows) else 1


if __name__ == "__main__":
    sys.exit(main())
