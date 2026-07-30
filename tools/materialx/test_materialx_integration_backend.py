"""Tests for the bounded Git MaterialX integration backend."""

from __future__ import annotations

from pathlib import Path
import tempfile
import unittest

from materialx_horde_dispatch import CommandResult
from materialx_integration_backend import (
    GitIntegrationBackend,
    IntegrationBackendError,
)


BASE = "a" * 40
HEAD = "b" * 40
FILES = ["intern/cycles/add.cpp"]


class FakeRunner:
    def __init__(self):
        self.results = []
        self.calls = []

    def add(self, returncode=0, stdout="", stderr=""):
        self.results.append(CommandResult(returncode, stdout, stderr))

    def __call__(self, command, *, cwd, input_text, timeout):
        self.calls.append({
            "command": tuple(command),
            "cwd": cwd,
            "input_text": input_text,
            "timeout": timeout,
        })
        if not self.results:
            raise AssertionError(f"unexpected command: {command!r}")
        return self.results.pop(0)


class GitIntegrationBackendTest(unittest.TestCase):
    def backend(self, directory, runner):
        root = Path(directory)
        repository = root / "repository"
        repository.mkdir()
        (repository / ".git").mkdir()
        worktrees = root / "worktrees"
        return GitIntegrationBackend(repository, worktrees, runner=runner)

    def prepare(self, backend, runner):
        runner.add(stdout=BASE + "\n")
        runner.add()
        return backend.prepare_worktree("native_cycles", "batch-a", BASE)

    def test_rejects_traversal_and_invalid_sha_before_runner(self):
        with tempfile.TemporaryDirectory() as directory:
            runner = FakeRunner()
            backend = self.backend(directory, runner)
            for layer, batch, base in (
                ("../native_cycles", "batch-a", BASE),
                ("native_cycles", "../batch", BASE),
                ("native_cycles", "batch-a", "not-a-sha"),
            ):
                with self.subTest(layer=layer, batch=batch, base=base):
                    with self.assertRaises(IntegrationBackendError):
                        backend.prepare_worktree(layer, batch, base)
            self.assertEqual(runner.calls, [])

    def test_prepare_requires_exact_declared_base(self):
        with tempfile.TemporaryDirectory() as directory:
            runner = FakeRunner()
            backend = self.backend(directory, runner)
            runner.add(stdout=HEAD + "\n")

            with self.assertRaisesRegex(IntegrationBackendError, "^stale_base$"):
                backend.prepare_worktree("native_cycles", "batch-a", BASE)

            self.assertEqual(len(runner.calls), 1)
            self.assertNotIn(HEAD, repr(runner.calls[0]["input_text"]))

    def test_failed_worktree_creation_attempts_cleanup(self):
        with tempfile.TemporaryDirectory() as directory:
            runner = FakeRunner()
            backend = self.backend(directory, runner)
            runner.add(stdout=BASE + "\n")
            runner.add(returncode=1, stderr="private failure")
            runner.add()

            with self.assertRaisesRegex(IntegrationBackendError, "^worktree_failure$"):
                backend.prepare_worktree("native_cycles", "batch-a", BASE)

            self.assertIn("remove", runner.calls[-1]["command"])

    def test_apply_exact_head_and_declared_diff_without_shell_interpolation(self):
        with tempfile.TemporaryDirectory() as directory:
            runner = FakeRunner()
            backend = self.backend(directory, runner)
            prepared = self.prepare(backend, runner)
            runner.add(stdout=HEAD + "\n")
            runner.add(stdout=FILES[0] + "\0")
            runner.add(stdout="binary patch")
            runner.add()

            result = backend.apply_artifact(
                prepared["worktree"],
                HEAD,
                FILES,
            )

            self.assertEqual(result, {
                "status": "applied",
                "head_sha": HEAD,
                "changed_files": FILES,
            })
            self.assertTrue(all(
                isinstance(call["command"], tuple) for call in runner.calls
            ))
            self.assertEqual(runner.calls[-1]["input_text"], "binary patch")
            self.assertNotIn("binary patch", repr(result))

    def test_stale_head_and_allowlist_escape_cleanup_isolation(self):
        for mode in ("stale_head", "allowlist_escape"):
            with self.subTest(mode=mode), tempfile.TemporaryDirectory() as directory:
                runner = FakeRunner()
                backend = self.backend(directory, runner)
                prepared = self.prepare(backend, runner)
                if mode == "stale_head":
                    runner.add(stdout=BASE + "\n")
                else:
                    runner.add(stdout=HEAD + "\n")
                    runner.add(stdout=FILES[0] + "\0source/escape.cpp\0")
                runner.add()

                with self.assertRaisesRegex(IntegrationBackendError, f"^{mode}$"):
                    backend.apply_artifact(prepared["worktree"], HEAD, FILES)

                self.assertEqual(
                    runner.calls[-1]["command"][3:6],
                    ("worktree", "remove", "--force"),
                )

    def test_apply_conflict_is_categorical_and_cleans_up(self):
        with tempfile.TemporaryDirectory() as directory:
            runner = FakeRunner()
            backend = self.backend(directory, runner)
            prepared = self.prepare(backend, runner)
            runner.add(stdout=HEAD + "\n")
            runner.add(stdout=FILES[0] + "\0")
            runner.add(stdout="patch carrying private source")
            runner.add(returncode=1, stderr="private conflict details")
            runner.add()

            result = backend.apply_artifact(prepared["worktree"], HEAD, FILES)

            self.assertEqual(result, {"status": "conflict"})
            self.assertNotIn("private", repr(result))
            self.assertIn("worktree", runner.calls[-1]["command"])
            self.assertIn("remove", runner.calls[-1]["command"])

    def test_runs_validated_commands_as_bounded_argv_and_cleans_on_failure(self):
        with tempfile.TemporaryDirectory() as directory:
            runner = FakeRunner()
            backend = self.backend(directory, runner)
            prepared = self.prepare(backend, runner)
            runner.add(stdout=HEAD + "\n")
            runner.add(stdout=FILES[0] + "\0")
            runner.add(stdout="patch")
            runner.add()
            backend.apply_artifact(prepared["worktree"], HEAD, FILES)
            runner.add(returncode=7, stdout="private output", stderr="private error")
            runner.add()

            result = backend.run_commands(
                prepared["worktree"],
                ["cycles_test --gtest_filter=MaterialXSemantic.Add"],
            )

            self.assertEqual(result, {
                "commands": [{
                    "command": "cycles_test --gtest_filter=MaterialXSemantic.Add",
                    "exit_code": 7,
                }]
            })
            command_call = runner.calls[-2]
            self.assertEqual(
                command_call["command"],
                ("cycles_test", "--gtest_filter=MaterialXSemantic.Add"),
            )
            self.assertNotIn("private", repr(result))
            self.assertIn("remove", runner.calls[-1]["command"])

    def test_merge_verifies_diff_updates_linear_ref_and_cleans(self):
        with tempfile.TemporaryDirectory() as directory:
            runner = FakeRunner()
            backend = self.backend(directory, runner)
            prepared = self.prepare(backend, runner)
            runner.add(stdout=HEAD + "\n")
            runner.add(stdout=FILES[0] + "\0")
            runner.add(stdout="patch")
            runner.add()
            backend.apply_artifact(prepared["worktree"], HEAD, FILES)
            runner.add(stdout=FILES[0] + "\0")
            runner.add(stdout="")
            runner.add(stdout="")
            runner.add(returncode=1)
            runner.add()
            runner.add()

            result = backend.merge_commit(
                prepared["worktree"],
                "native_cycles",
                "batch-a",
                HEAD,
            )

            self.assertEqual(result, {"status": "merged", "head_sha": HEAD})
            update = next(
                call for call in runner.calls
                if "update-ref" in call["command"]
            )
            self.assertEqual(update["command"][-1], "0" * 40)
            self.assertIn("remove", runner.calls[-1]["command"])

    def test_merge_failure_is_categorical_and_cleans(self):
        with tempfile.TemporaryDirectory() as directory:
            runner = FakeRunner()
            backend = self.backend(directory, runner)
            prepared = self.prepare(backend, runner)
            runner.add(stdout=HEAD + "\n")
            runner.add(stdout=FILES[0] + "\0")
            runner.add(stdout="patch")
            runner.add()
            backend.apply_artifact(prepared["worktree"], HEAD, FILES)
            runner.add(stdout=FILES[0] + "\0")
            runner.add(stdout="")
            runner.add(stdout="")
            runner.add(returncode=1)
            runner.add(returncode=1, stderr="private merge failure")
            runner.add()

            result = backend.merge_commit(
                prepared["worktree"],
                "native_cycles",
                "batch-a",
                HEAD,
            )

            self.assertEqual(result, {"status": "failure"})
            self.assertNotIn("private", repr(result))
            self.assertIn("remove", runner.calls[-1]["command"])

    def test_merge_rejects_unstaged_or_untracked_allowlist_escape(self):
        for kind in ("unstaged", "untracked"):
            with self.subTest(kind=kind), tempfile.TemporaryDirectory() as directory:
                runner = FakeRunner()
                backend = self.backend(directory, runner)
                prepared = self.prepare(backend, runner)
                runner.add(stdout=HEAD + "\n")
                runner.add(stdout=FILES[0] + "\0")
                runner.add(stdout="patch")
                runner.add()
                backend.apply_artifact(prepared["worktree"], HEAD, FILES)
                runner.add(stdout=FILES[0] + "\0")
                runner.add(stdout="source/escape.cpp\0" if kind == "unstaged" else "")
                if kind == "untracked":
                    runner.add(stdout="source/escape.cpp\0")
                runner.add()

                result = backend.merge_commit(
                    prepared["worktree"],
                    "native_cycles",
                    "batch-a",
                    HEAD,
                )

                self.assertEqual(result, {"status": "failure"})
                self.assertFalse(any(
                    "update-ref" in call["command"] for call in runner.calls
                ))
                self.assertIn("remove", runner.calls[-1]["command"])


if __name__ == "__main__":
    unittest.main()
