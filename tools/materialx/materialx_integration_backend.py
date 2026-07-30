#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""Bounded Git operations for isolated MaterialX integration trains."""

from __future__ import annotations

__all__ = ("GitIntegrationBackend", "IntegrationBackendError")

from dataclasses import dataclass
from pathlib import Path, PurePosixPath
import re
import shlex
import subprocess
from typing import Any, Callable, Sequence

from materialx_horde_dispatch import CommandResult
from materialx_integration_train import LAYERS


COMMAND_TIMEOUT_SECONDS = 900
_IDENTIFIER = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$")
_SHA = re.compile(r"^[0-9a-f]{40}$")
_ZERO_SHA = "0" * 40
IntegrationRunner = Callable[..., CommandResult]


class IntegrationBackendError(ValueError):
    """A fixed categorical integration failure with no remote detail."""

    def __init__(self, classification: str):
        self.classification = classification
        super().__init__(classification)


@dataclass
class _Worktree:
    path: Path
    layer: str
    batch_id: str
    base_sha: str
    head_sha: str = ""
    changed_files: tuple[str, ...] = ()


def _subprocess_runner(
    command: Sequence[str],
    *,
    cwd: str,
    input_text: str | None,
    timeout: int,
) -> CommandResult:
    try:
        result = subprocess.run(
            tuple(command),
            cwd=cwd,
            input=input_text,
            text=True,
            capture_output=True,
            timeout=timeout,
            check=False,
            shell=False,
        )
    except (OSError, subprocess.SubprocessError):
        return CommandResult(-1, "", "")
    return CommandResult(result.returncode, result.stdout, result.stderr)


def _identifier(value: Any, classification: str) -> str:
    if not isinstance(value, str) or not _IDENTIFIER.fullmatch(value):
        raise IntegrationBackendError(classification)
    return value


def _sha(value: Any, classification: str) -> str:
    if not isinstance(value, str) or not _SHA.fullmatch(value):
        raise IntegrationBackendError(classification)
    return value


def _changed_files(value: Any) -> tuple[str, ...]:
    if isinstance(value, (str, bytes)) or not isinstance(value, Sequence) or not value:
        raise IntegrationBackendError("invalid_changed_files")
    result = []
    for item in value:
        if not isinstance(item, str) or not item or "\\" in item or "\0" in item:
            raise IntegrationBackendError("invalid_changed_files")
        path = PurePosixPath(item)
        if path.is_absolute() or ".." in path.parts or "." in path.parts or path.as_posix() != item:
            raise IntegrationBackendError("invalid_changed_files")
        result.append(item)
    if result != sorted(set(result)):
        raise IntegrationBackendError("invalid_changed_files")
    return tuple(result)


def _nul_paths(output: str) -> tuple[str, ...] | None:
    if not isinstance(output, str) or len(output) > 1_000_000:
        return None
    values = output.split("\0")
    if values and values[-1] == "":
        values.pop()
    try:
        return _changed_files(values)
    except IntegrationBackendError:
        return None


class GitIntegrationBackend:
    """Apply and advance validated artifacts without shell interpolation."""

    def __init__(
        self,
        repository_root: str | Path,
        worktree_root: str | Path,
        *,
        runner: IntegrationRunner | None = None,
    ):
        repository = Path(repository_root).resolve()
        worktrees = Path(worktree_root).resolve()
        if not repository.is_dir() or not (repository / ".git").exists():
            raise ValueError("repository_root must be a Git repository")
        if repository == worktrees or repository in worktrees.parents or worktrees in repository.parents:
            raise ValueError("repository and worktree roots must be disjoint")
        worktrees.mkdir(parents=True, exist_ok=True)
        self._repository = repository
        self._worktrees = worktrees
        self._runner = runner or _subprocess_runner
        self._contexts: dict[str, _Worktree] = {}

    def _run(
        self,
        command: Sequence[str],
        *,
        cwd: Path,
        input_text: str | None = None,
    ) -> CommandResult:
        try:
            result = self._runner(
                tuple(command),
                cwd=str(cwd),
                input_text=input_text,
                timeout=COMMAND_TIMEOUT_SECONDS,
            )
        except Exception:
            return CommandResult(-1, "", "")
        if not isinstance(result, CommandResult):
            return CommandResult(-1, "", "")
        return result

    def _git(
        self,
        root: Path,
        *arguments: str,
        input_text: str | None = None,
    ) -> CommandResult:
        return self._run(
            ("git", "-C", str(root), *arguments),
            cwd=self._repository,
            input_text=input_text,
        )

    def _context(self, worktree: str) -> _Worktree:
        if not isinstance(worktree, str):
            raise IntegrationBackendError("unknown_worktree")
        context = self._contexts.get(worktree)
        if context is None or str(context.path) != worktree:
            raise IntegrationBackendError("unknown_worktree")
        return context

    def _cleanup(self, context: _Worktree) -> None:
        self._git(
            self._repository,
            "worktree",
            "remove",
            "--force",
            str(context.path),
        )
        self._contexts.pop(str(context.path), None)

    def prepare_worktree(
        self,
        layer: str,
        batch_id: str,
        base_sha: str,
    ) -> dict[str, str]:
        if layer not in LAYERS:
            raise IntegrationBackendError("invalid_layer")
        batch_id = _identifier(batch_id, "invalid_batch")
        base_sha = _sha(base_sha, "invalid_base")
        worktree = (self._worktrees / layer / batch_id).resolve()
        if self._worktrees not in worktree.parents or worktree.exists():
            raise IntegrationBackendError("invalid_worktree")
        verified = self._git(
            self._repository,
            "rev-parse",
            "--verify",
            f"{base_sha}^{{commit}}",
        )
        if verified.returncode != 0 or verified.stdout.strip().lower() != base_sha:
            raise IntegrationBackendError("stale_base")
        worktree.parent.mkdir(parents=True, exist_ok=True)
        added = self._git(
            self._repository,
            "worktree",
            "add",
            "--detach",
            str(worktree),
            base_sha,
        )
        if added.returncode != 0:
            self._git(
                self._repository,
                "worktree",
                "remove",
                "--force",
                str(worktree),
            )
            raise IntegrationBackendError("worktree_failure")
        context = _Worktree(worktree, layer, batch_id, base_sha)
        self._contexts[str(worktree)] = context
        return {"worktree": str(worktree), "base_sha": base_sha}

    def apply_artifact(
        self,
        worktree: str,
        head_sha: str,
        changed_files: Sequence[str],
    ) -> dict[str, Any]:
        context = self._context(worktree)
        try:
            head_sha = _sha(head_sha, "invalid_head")
            files = _changed_files(changed_files)
            verified = self._git(
                self._repository,
                "rev-parse",
                "--verify",
                f"{head_sha}^{{commit}}",
            )
            if verified.returncode != 0 or verified.stdout.strip().lower() != head_sha:
                raise IntegrationBackendError("stale_head")
            diff = self._git(
                self._repository,
                "diff",
                "--name-only",
                "-z",
                context.base_sha,
                head_sha,
            )
            actual_files = _nul_paths(diff.stdout) if diff.returncode == 0 else None
            if actual_files is None or actual_files != files:
                raise IntegrationBackendError("allowlist_escape")
            patch = self._git(
                self._repository,
                "diff",
                "--binary",
                context.base_sha,
                head_sha,
                "--",
                *files,
            )
            if patch.returncode != 0:
                raise IntegrationBackendError("artifact_failure")
            applied = self._git(
                context.path,
                "apply",
                "--index",
                "--whitespace=nowarn",
                "-",
                input_text=patch.stdout,
            )
            if applied.returncode != 0:
                self._cleanup(context)
                return {"status": "conflict"}
            context.head_sha = head_sha
            context.changed_files = files
            return {
                "status": "applied",
                "head_sha": head_sha,
                "changed_files": list(files),
            }
        except IntegrationBackendError:
            self._cleanup(context)
            raise

    def run_commands(
        self,
        worktree: str,
        focused_commands: Sequence[str],
    ) -> dict[str, list[dict[str, Any]]]:
        context = self._context(worktree)
        if (
            isinstance(focused_commands, (str, bytes))
            or not isinstance(focused_commands, Sequence)
            or not focused_commands
            or len(focused_commands) > 64
        ):
            self._cleanup(context)
            raise IntegrationBackendError("invalid_commands")
        results = []
        for command in focused_commands:
            if not isinstance(command, str) or not command or len(command) > 4_096 or "\0" in command:
                self._cleanup(context)
                raise IntegrationBackendError("invalid_commands")
            try:
                arguments = tuple(shlex.split(command, posix=True))
            except ValueError:
                arguments = ()
            if not arguments:
                self._cleanup(context)
                raise IntegrationBackendError("invalid_commands")
            result = self._run(
                arguments,
                cwd=context.path,
                input_text=None,
            )
            exit_code = (
                result.returncode
                if isinstance(result.returncode, int)
                and not isinstance(result.returncode, bool)
                else -1
            )
            results.append({"command": command, "exit_code": exit_code})
            if exit_code != 0:
                self._cleanup(context)
                break
        return {"commands": results}

    def merge_commit(
        self,
        worktree: str,
        layer: str,
        batch_id: str,
        head_sha: str,
    ) -> dict[str, str]:
        context = self._context(worktree)
        outcome = {"status": "failure"}
        try:
            if (
                layer != context.layer
                or _identifier(batch_id, "invalid_batch") != context.batch_id
                or _sha(head_sha, "invalid_head") != context.head_sha
            ):
                return outcome
            staged = self._git(
                context.path,
                "diff",
                "--cached",
                "--name-only",
                "-z",
            )
            if (
                staged.returncode != 0
                or _nul_paths(staged.stdout) != context.changed_files
            ):
                return outcome
            unstaged = self._git(
                context.path,
                "diff",
                "--name-only",
                "-z",
            )
            if unstaged.returncode != 0 or unstaged.stdout:
                return outcome
            untracked = self._git(
                context.path,
                "ls-files",
                "--others",
                "--exclude-standard",
                "-z",
            )
            if untracked.returncode != 0 or untracked.stdout:
                return outcome
            reference = f"refs/heads/materialx-integration/{layer}"
            current = self._git(
                self._repository,
                "show-ref",
                "--verify",
                "--hash",
                reference,
            )
            if current.returncode == 0:
                old_sha = current.stdout.strip().lower()
                if old_sha != context.base_sha:
                    return outcome
            elif current.returncode == 1:
                old_sha = _ZERO_SHA
            else:
                return outcome
            updated = self._git(
                self._repository,
                "update-ref",
                reference,
                head_sha,
                old_sha,
            )
            if updated.returncode != 0:
                return outcome
            outcome = {"status": "merged", "head_sha": head_sha}
            return outcome
        except IntegrationBackendError:
            return outcome
        finally:
            self._cleanup(context)
