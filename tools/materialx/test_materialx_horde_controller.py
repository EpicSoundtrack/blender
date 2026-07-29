"""Tests for the fail-closed Horde harvest-and-refill controller."""

from __future__ import annotations

import unittest

from materialx_horde_controller import build_refill_cycle


class MaterialXHordeControllerTest(unittest.TestCase):
    def test_assigns_next_distinct_batches_to_all_healthy_idle_workers(self) -> None:
        result = build_refill_cycle(
            workers=[
                {"id": "blend05", "state": "idle", "harvest": "success"},
                {"id": "blendit", "state": "active", "harvest": "pending"},
                {"id": "blendit2", "state": "idle", "harvest": "success"},
            ],
            queued_batches=[
                {"batch_id": "batch-a", "prompt": "MaterialX task A"},
                {"batch_id": "batch-b", "prompt": "MaterialX task B"},
            ],
        )

        self.assertEqual(result["completed_workers"], ["blend05", "blendit2"])
        self.assertEqual(
            result["refills"],
            [
                {"worker_id": "blend05", "batch_id": "batch-a", "prompt": "MaterialX task A"},
                {"worker_id": "blendit2", "batch_id": "batch-b", "prompt": "MaterialX task B"},
            ],
        )
        self.assertEqual(result["blocked_workers"], [])

    def test_fails_closed_when_a_healthy_idle_worker_has_no_harvest_evidence(self) -> None:
        result = build_refill_cycle(
            workers=[{"id": "blend05", "state": "idle", "harvest": "missing"}],
            queued_batches=[{"batch_id": "batch-a", "prompt": "MaterialX task A"}],
        )

        self.assertEqual(result["refills"], [])
        self.assertEqual(result["blocked_workers"], ["blend05"])
        self.assertEqual(result["alerts"][0]["classification"], "harvest_missing")


if __name__ == "__main__":
    unittest.main()
