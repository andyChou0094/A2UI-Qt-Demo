"""Stateless SurfaceSpec document import, export, and default loading."""

import json
from pathlib import Path

from fastapi import APIRouter, Request
from fastapi.responses import JSONResponse, Response

from backend.agent_service.surface_validator import validate_surface_spec


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_SURFACE_PATH = ROOT / "shared" / "default-surface.json"
MAX_SURFACE_DOCUMENT_BYTES = 64 * 1024


router = APIRouter(tags=["surface-documents"])


def invalid_surface_document(diagnostics):
    diagnostics = [str(item) for item in diagnostics]
    return JSONResponse(
        status_code=422,
        content={
            "error": {
                "code": "invalid_surface_document",
                "message": diagnostics[0] if diagnostics else "invalid SurfaceSpec document",
                "diagnostics": diagnostics,
            }
        },
    )


def parse_and_validate_surface_document(payload):
    if len(payload) > MAX_SURFACE_DOCUMENT_BYTES:
        return None, ["surface document exceeds 64 KiB"]
    try:
        text = payload.decode("utf-8")
    except UnicodeDecodeError as error:
        return None, ["surface document must be UTF-8: " + str(error)]
    try:
        document = json.loads(text)
    except ValueError as error:
        return None, ["invalid JSON: " + str(error)]
    diagnostics = validate_surface_spec(document)
    return document, diagnostics


def load_default_surface(path=None):
    default_path = Path(path) if path else DEFAULT_SURFACE_PATH
    payload = default_path.read_bytes()
    document, diagnostics = parse_and_validate_surface_document(payload)
    if diagnostics:
        raise RuntimeError(
            "default SurfaceSpec is invalid: " + "; ".join(diagnostics)
        )
    return document


def canonical_surface_json(document):
    return (json.dumps(document, ensure_ascii=False, indent=2) + "\n").encode("utf-8")


@router.post("/surface/import")
async def import_surface(request: Request):
    document, diagnostics = parse_and_validate_surface_document(await request.body())
    if diagnostics:
        return invalid_surface_document(diagnostics)
    return {"surface": document}


@router.post("/surface/export")
async def export_surface(request: Request):
    document, diagnostics = parse_and_validate_surface_document(await request.body())
    if diagnostics:
        return invalid_surface_document(diagnostics)
    return Response(
        content=canonical_surface_json(document),
        media_type="application/json",
        headers={"Content-Disposition": 'attachment; filename="surface-main.json"'},
    )


@router.get("/surface/default")
def default_surface(request: Request):
    return {"surface": request.app.state.default_surface}
