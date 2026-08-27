"""HTTP composition contract, isolated from calculation business persistence."""

import json

from fastapi import APIRouter, Request
from fastapi.responses import JSONResponse
from pydantic import BaseModel, ConfigDict, Field

from backend.agent_service.layout_plan import CompositionError


class ClosedModel(BaseModel):
    model_config = ConfigDict(extra="forbid")


class ComposeRequest(ClosedModel):
    __annotations__ = {"prompt": str, "currentSurface": dict}
    prompt = Field(min_length=1)


router = APIRouter(tags=["agent-composition"])


def error_response(error):
    return JSONResponse(
        status_code=422,
        content={
            "error": {
                "code": error.code,
                "message": error.diagnostics[0] if error.diagnostics else error.code,
                "diagnostics": error.diagnostics,
            }
        },
    )


@router.post("/compose")
def compose(payload: ComposeRequest, request: Request):
    try:
        plan = request.app.state.llm_adapter.generate_layout_plan(
            payload.prompt,
            payload.currentSurface,
            request.app.state.effective_catalog,
        )
        surface = request.app.state.surface_compiler.compile(
            plan, payload.currentSurface
        )
    except CompositionError as error:
        return error_response(error)
    except Exception as error:
        return error_response(CompositionError("composition_failed", [str(error)]))
    return {"surface": surface}
