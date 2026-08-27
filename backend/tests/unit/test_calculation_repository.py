from pathlib import Path
from tempfile import TemporaryDirectory
import unittest

from backend.mock_business_api.repository import CalculationRepository


class CalculationRepositoryTest(unittest.TestCase):
    def test_file_database_survives_repository_restart(self):
        with TemporaryDirectory() as directory:
            database_path = Path(directory) / "calculations.sqlite3"
            first_repository = CalculationRepository(database_path)
            created = first_repository.create("6*7", 42.0)
            first_repository.close()

            second_repository = CalculationRepository(database_path)
            restored = second_repository.get(created.id)
            second_repository.close()

            self.assertIsNotNone(restored)
            self.assertEqual(restored.expression, "6*7")
            self.assertEqual(restored.result, 42.0)
            self.assertEqual(restored.note, "")

    def test_each_test_database_is_isolated(self):
        with TemporaryDirectory() as first_directory, TemporaryDirectory() as second_directory:
            first = CalculationRepository(Path(first_directory) / "calculations.sqlite3")
            second = CalculationRepository(Path(second_directory) / "calculations.sqlite3")
            first.create("1+1", 2.0)

            self.assertEqual(len(first.list_recent()), 1)
            self.assertEqual(second.list_recent(), [])
            first.close()
            second.close()

    def test_crud_and_summary_stay_inside_repository(self):
        with TemporaryDirectory() as directory:
            with CalculationRepository(Path(directory) / "calculations.sqlite3") as repository:
                older = repository.create("1+2", 3.0)
                newer = repository.create("3+4", 7.0)

                self.assertEqual(
                    [record.id for record in repository.list_recent(2)],
                    [newer.id, older.id],
                )
                updated = repository.update_note(older.id, "checked")
                self.assertEqual(updated.note, "checked")
                self.assertEqual(repository.summary()["count"], 2)
                self.assertEqual(repository.summary()["latest"].id, newer.id)
                self.assertTrue(repository.delete(newer.id))
                self.assertFalse(repository.delete("missing"))
                self.assertEqual(repository.summary()["count"], 1)

    def test_query_limit_is_bounded(self):
        with TemporaryDirectory() as directory:
            with CalculationRepository(Path(directory) / "calculations.sqlite3") as repository:
                for invalid_limit in (0, 51, True, "50"):
                    with self.assertRaises(ValueError):
                        repository.list_recent(invalid_limit)


if __name__ == "__main__":
    unittest.main()
