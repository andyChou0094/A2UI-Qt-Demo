"""The only module allowed to access the calculation SQLite database."""

from datetime import datetime, timezone
import sqlite3
import threading
import uuid

from .models import record_from_row


class CalculationRepository(object):
    """Owns one SQLite connection and serializes all access to it."""

    def __init__(self, database_path):
        self._database_path = str(database_path)
        self._lock = threading.RLock()
        self._connection = sqlite3.connect(
            self._database_path,
            check_same_thread=False,
        )
        self._connection.row_factory = sqlite3.Row
        self._initialize()

    @property
    def database_path(self):
        return self._database_path

    def close(self):
        with self._lock:
            if self._connection is not None:
                self._connection.close()
                self._connection = None

    def create(self, expression, result):
        record_id = str(uuid.uuid4())
        timestamp = _utc_timestamp()
        with self._lock:
            connection = self._require_connection()
            connection.execute(
                """
                INSERT INTO calculations
                    (id, expression, result, note, created_at, updated_at)
                VALUES (?, ?, ?, ?, ?, ?)
                """,
                (record_id, expression, float(result), "", timestamp, timestamp),
            )
            connection.commit()
            return self.get(record_id)

    def get(self, record_id):
        with self._lock:
            row = self._require_connection().execute(
                """
                SELECT id, expression, result, note, created_at, updated_at
                FROM calculations
                WHERE id = ?
                """,
                (record_id,),
            ).fetchone()
            return record_from_row(row) if row is not None else None

    def list_recent(self, limit=50):
        if isinstance(limit, bool) or not isinstance(limit, int) or limit < 1 or limit > 50:
            raise ValueError("limit must be an integer between 1 and 50")
        with self._lock:
            rows = self._require_connection().execute(
                """
                SELECT id, expression, result, note, created_at, updated_at
                FROM calculations
                ORDER BY created_at DESC, rowid DESC
                LIMIT ?
                """,
                (limit,),
            ).fetchall()
            return [record_from_row(row) for row in rows]

    def update_note(self, record_id, note):
        timestamp = _utc_timestamp()
        with self._lock:
            connection = self._require_connection()
            cursor = connection.execute(
                """
                UPDATE calculations
                SET note = ?, updated_at = ?
                WHERE id = ?
                """,
                (note, timestamp, record_id),
            )
            connection.commit()
            return self.get(record_id) if cursor.rowcount else None

    def delete(self, record_id):
        with self._lock:
            connection = self._require_connection()
            cursor = connection.execute(
                "DELETE FROM calculations WHERE id = ?",
                (record_id,),
            )
            connection.commit()
            return cursor.rowcount > 0

    def summary(self):
        with self._lock:
            connection = self._require_connection()
            count = connection.execute(
                "SELECT COUNT(*) AS count FROM calculations"
            ).fetchone()["count"]
            latest = connection.execute(
                """
                SELECT id, expression, result, note, created_at, updated_at
                FROM calculations
                ORDER BY created_at DESC, rowid DESC
                LIMIT 1
                """
            ).fetchone()
            return {
                "count": count,
                "latest": record_from_row(latest) if latest is not None else None,
            }

    def _initialize(self):
        with self._lock:
            connection = self._require_connection()
            connection.execute(
                """
                CREATE TABLE IF NOT EXISTS calculations (
                    id TEXT PRIMARY KEY,
                    expression TEXT NOT NULL,
                    result REAL NOT NULL,
                    note TEXT NOT NULL DEFAULT '',
                    created_at TEXT NOT NULL,
                    updated_at TEXT NOT NULL
                )
                """
            )
            connection.commit()

    def _require_connection(self):
        if self._connection is None:
            raise RuntimeError("repository is closed")
        return self._connection

    def __enter__(self):
        return self

    def __exit__(self, exception_type, exception, traceback):
        self.close()


def _utc_timestamp():
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace(
        "+00:00", "Z"
    )
