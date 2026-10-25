"""Backup changes must select their real CTest regression in FAST and IMPACT."""
import unittest
from test_turbo import fake_changes, fake_inventory, fake_test
from turbo_lib import selection


class BackupSelectionTests(unittest.TestCase):
    def test_backup_sources_select_registered_regression(self):
        data = fake_inventory([fake_test("linblux_backup", "", ("tooling", "backup"))])
        for mode in ("fast", "impact"):
            for path in ("tools/backup/linblux_backup/service.py", "tests/backup/test_backup.py",
                         "packaging/systemd/linblux-backup.service.in"):
                with self.subTest(mode=mode, path=path):
                    plan = selection.build_plan(data, fake_changes(path, "tooling"), mode)
                    self.assertIn("linblux_backup", plan["selected_names"])


if __name__ == "__main__":
    unittest.main()
