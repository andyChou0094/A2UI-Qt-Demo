"""Standalone Agent Service application factory."""

import json
from pathlib import Path

from fastapi import FastAPI

from backend.agent_service.layout_plan import SurfaceCompiler
from backend.agent_service.llm_adapter import adapter_from_environment
from backend.agent_service.router import router
from backend.agent_service.surface_documents import load_default_surface
from backend.agent_service.surface_documents import router as surface_document_router


ROOT = Path(__file__).resolve().parents[2]


def create_agent_app(llm_adapter=None, surface_compiler=None):
    application = FastAPI(
        title="A2UI Controlled Composition Agent Service",
        version="0.1.0",
    )
    application.state.llm_adapter = llm_adapter or adapter_from_environment()
    application.state.surface_compiler = surface_compiler or SurfaceCompiler()
    application.state.effective_catalog = json.loads(
        (ROOT / "shared" / "catalog" / "component-catalog.json").read_text(
            encoding="utf-8"
        )
    )
    application.state.default_surface = load_default_surface()
    application.include_router(router)
    application.include_router(surface_document_router)
    return application
