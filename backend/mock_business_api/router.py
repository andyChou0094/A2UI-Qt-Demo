"""FastAPI router for the isolated calculation business API."""

from typing import List, Optional

from fastapi import APIRouter, Depends, HTTPException, Query, Request, Response, status
from pydantic import BaseModel, ConfigDict, Field

from .repository import CalculationRepository


class ClosedModel(BaseModel):
    model_config = ConfigDict(extra="forbid")


class CreateCalculationRequest(ClosedModel):
    expression: str = Field(min_length=1)
    result: float


class UpdateNoteRequest(ClosedModel):
    note: str


class CalculationRecordResponse(ClosedModel):
    id: str
    expression: str
    result: float
    note: str
    createdAt: str
    updatedAt: str


class CalculationSummaryResponse(ClosedModel):
    count: int
    latest: Optional[CalculationRecordResponse]


def get_repository(request: Request):
    repository = getattr(request.app.state, "calculation_repository", None)
    if not isinstance(repository, CalculationRepository):
        raise RuntimeError("calculation repository is not configured")
    return repository


router = APIRouter(prefix="/api/calculations", tags=["calculation-business-api"])


@router.post(
    "",
    response_model=CalculationRecordResponse,
    status_code=status.HTTP_201_CREATED,
)
def create_calculation(
    payload: CreateCalculationRequest,
    repository: CalculationRepository = Depends(get_repository),
):
    return repository.create(payload.expression, payload.result).to_dict()


@router.get("", response_model=List[CalculationRecordResponse])
def list_calculations(
    limit: int = Query(default=50, ge=1, le=50),
    repository: CalculationRepository = Depends(get_repository),
):
    return [record.to_dict() for record in repository.list_recent(limit)]


@router.patch("/{record_id}", response_model=CalculationRecordResponse)
def update_calculation_note(
    record_id: str,
    payload: UpdateNoteRequest,
    repository: CalculationRepository = Depends(get_repository),
):
    record = repository.update_note(record_id, payload.note)
    if record is None:
        raise HTTPException(status_code=404, detail="calculation record not found")
    return record.to_dict()


@router.delete("/{record_id}", status_code=status.HTTP_204_NO_CONTENT)
def delete_calculation(
    record_id: str,
    repository: CalculationRepository = Depends(get_repository),
):
    if not repository.delete(record_id):
        raise HTTPException(status_code=404, detail="calculation record not found")
    return Response(status_code=status.HTTP_204_NO_CONTENT)


@router.get("/summary", response_model=CalculationSummaryResponse)
def calculation_summary(
    repository: CalculationRepository = Depends(get_repository),
):
    summary = repository.summary()
    return {
        "count": summary["count"],
        "latest": summary["latest"].to_dict() if summary["latest"] else None,
    }
