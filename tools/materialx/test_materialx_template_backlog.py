import unittest
import materialx_template_backlog as backlog


class TemplateBacklogTest(unittest.TestCase):
    def setUp(self):
        self.catalog = [{"id": "ND_a"}, {"id": "ND_b"}]

    def test_conservative_complete_deterministic_classification(self):
        rows = backlog.build_backlog(self.catalog, {"ND_b": {"classification": "direct_template"}})
        self.assertEqual(rows, [{"id": "ND_a", "classification": "renderer_specific", "next_action": "classify"},
                                {"id": "ND_b", "classification": "direct_template", "next_action": "template"}])

    def test_rejects_duplicate_missing_and_unknown_ids(self):
        with self.assertRaisesRegex(ValueError, "duplicate"):
            backlog.build_backlog([{"id": "ND_a"}, {"id": "ND_a"}])
        with self.assertRaisesRegex(ValueError, "unknown"):
            backlog.build_backlog(self.catalog, {"ND_missing": {}})
        with self.assertRaisesRegex(ValueError, "Unknown classification"):
            backlog.build_backlog(self.catalog, {"ND_a": {"classification": "bad"}})
