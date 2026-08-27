"""Calculation business records and their public JSON representation."""

from collections import namedtuple


_RecordBase = namedtuple(
    "CalculationRecord",
    ("id", "expression", "result", "note", "created_at", "updated_at"),
)


class CalculationRecord(_RecordBase):
    __slots__ = ()

    def to_dict(self):
        return {
            "id": self.id,
            "expression": self.expression,
            "result": self.result,
            "note": self.note,
            "createdAt": self.created_at,
            "updatedAt": self.updated_at,
        }


def record_from_row(row):
    return CalculationRecord(
        id=row["id"],
        expression=row["expression"],
        result=row["result"],
        note=row["note"],
        created_at=row["created_at"],
        updated_at=row["updated_at"],
    )
