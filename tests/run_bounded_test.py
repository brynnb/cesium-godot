"""Run one engine test with a wall-clock limit and reject script errors.

Only the child process created here is killed on timeout. Expected engine
diagnostics from negative tests are retained; GDScript runtime/parse errors are
always test failures, even when Godot returns exit status zero.
"""
import os
import subprocess
import sys
import time


def run(command, timeout):
    started = time.monotonic()
    print("Test start: " + " ".join(command), flush=True)
    try:
        result = subprocess.run(command, capture_output=True, timeout=timeout)
    except subprocess.TimeoutExpired as error:
        sys.stdout.buffer.write(error.stdout or b"")
        sys.stderr.buffer.write(error.stderr or b"")
        print(f"Test timed out after {timeout:g}s", file=sys.stderr, flush=True)
        return 124
    sys.stdout.buffer.write(result.stdout)
    sys.stderr.buffer.write(result.stderr)
    status = result.returncode
    if b"SCRIPT ERROR:" in result.stdout + result.stderr:
        status = status or 1
    print(f"Test finished: {time.monotonic() - started:.2f}s exit={status}", flush=True)
    return status


if __name__ == "__main__":
    if len(sys.argv) < 2:
        raise SystemExit("Usage: run_bounded_test.py executable [arguments...]")
    timeout = float(os.environ.get("GODOT_TEST_TIMEOUT_SECONDS", "180"))
    if not 0 < timeout < float("inf"):
        raise SystemExit("GODOT_TEST_TIMEOUT_SECONDS must be finite and positive")
    raise SystemExit(run(sys.argv[1:], timeout))
