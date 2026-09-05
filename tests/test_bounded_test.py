import subprocess
import sys
import os
import unittest
from pathlib import Path


class BoundedTest(unittest.TestCase):
    def invoke(self, code, timeout="5"):
        return subprocess.run(
            [sys.executable, str(Path(__file__).with_name("run_bounded_test.py")), sys.executable, "-c", code],
            env={**os.environ, "GODOT_TEST_TIMEOUT_SECONDS": timeout},
            capture_output=True, timeout=10,
        )

    def test_success(self):
        self.assertEqual(self.invoke("print('passed')").returncode, 0)

    def test_script_error_with_zero_exit(self):
        self.assertNotEqual(self.invoke("print('SCRIPT ERROR: broken')").returncode, 0)

    def test_nonzero_exit(self):
        self.assertEqual(self.invoke("raise SystemExit(3)").returncode, 3)

    def test_timeout(self):
        result = self.invoke("import time; time.sleep(60)", "0.1")
        self.assertEqual(result.returncode, 124)
        self.assertIn(b"timed out", result.stderr)
