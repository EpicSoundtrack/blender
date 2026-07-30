"""Tests for categorical, fail-closed worker source preflight."""

from __future__ import annotations

import unittest

from materialx_worker_preflight import REQUIRED_ARCHITECTURE_FILES, parse_probe_document, preflight_workers


SHA = "a" * 40


class FakeProbe:
    def __init__(self, heads, *, repository_present=True, files=None):
        self.heads = {worker: list(values) if isinstance(values, tuple) else [values] for worker, values in heads.items()}
        self.repository_present = repository_present
        self.files = files if files is not None else {path: True for path in REQUIRED_ARCHITECTURE_FILES}
        self.calls = []

    def probe(self, worker):
        self.calls.append(worker)
        head = self.heads[worker].pop(0)
        return {"repository_present": self.repository_present, "files": self.files, "head": head}


class FakeSynchronizer:
    def __init__(self):
        self.calls = []

    def synchronize(self, worker, expected_sha):
        self.calls.append((worker, expected_sha))
        return True


class MaterialXWorkerPreflightTest(unittest.TestCase):
    def test_missing_repository_is_blocked_without_synchronization(self):
        probe = FakeProbe({"blend05": SHA}, repository_present=False)

        result = preflight_workers(["blend05"], SHA, probe=probe, synchronizer=FakeSynchronizer())

        self.assertEqual(result["blend05"]["state"], "missing_repository")

    def test_missing_required_architecture_file_is_blocked(self):
        files = {path: True for path in REQUIRED_ARCHITECTURE_FILES}
        files[REQUIRED_ARCHITECTURE_FILES[0]] = False

        result = preflight_workers(["blend05"], SHA, probe=FakeProbe({"blend05": SHA}, files=files))

        self.assertEqual(result["blend05"]["state"], "missing_required_file")

    def test_stale_worker_is_synchronized_and_must_match_on_reprobe(self):
        sync = FakeSynchronizer()
        probe = FakeProbe({"blend05": ("b" * 40, SHA)})

        result = preflight_workers(["blend05"], SHA, probe=probe, synchronizer=sync)

        self.assertEqual(result["blend05"]["state"], "ready")
        self.assertEqual(sync.calls, [("blend05", SHA)])
        self.assertEqual(probe.calls, ["blend05", "blend05"])

    def test_mismatched_reprobe_remains_blocked(self):
        probe = FakeProbe({"blend05": ("b" * 40, "c" * 40)})

        result = preflight_workers(["blend05"], SHA, probe=probe, synchronizer=FakeSynchronizer())

        self.assertEqual(result["blend05"]["state"], "stale_source")

    def test_malformed_probe_is_fail_closed(self):
        class MalformedProbe:
            def probe(self, worker):
                return {"repository_present": True, "files": {}, "head": SHA, "unsafe": "log"}

        result = preflight_workers(["blend05"], SHA, probe=MalformedProbe())

        self.assertEqual(result["blend05"], {"state": "invalid_probe"})

    def test_parser_rejects_non_sentinel_output(self):
        with self.assertRaisesRegex(ValueError, "source probe output"):
            parse_probe_document("repository_present=true\nHEAD=" + SHA)

    def test_stale_worker_does_not_block_four_healthy_workers(self):
        workers = ["blend05", "blendit04", "blendit", "blendit2", "blendit3"]
        probe = FakeProbe({worker: SHA for worker in workers})
        probe.heads["blendit04"] = ["b" * 40]

        result = preflight_workers(workers, SHA, probe=probe)

        self.assertEqual(result["blendit04"]["state"], "stale_source")
        self.assertEqual({worker for worker, status in result.items() if status["state"] == "ready"}, set(workers) - {"blendit04"})


if __name__ == "__main__":
    unittest.main()
