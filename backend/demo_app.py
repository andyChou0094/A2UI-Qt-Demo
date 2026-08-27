"""Runnable composition root for the local-loopback demo process."""

import json
import os
from contextlib import asynccontextmanager
from pathlib import Path

from fastapi import FastAPI

from backend.agent_service.layout_plan import SurfaceCompiler
from backend.agent_service.llm_adapter import adapter_from_environment
from backend.agent_service.router import router as composition_router
from backend.agent_service.surface_documents import load_default_surface
from backend.agent_service.surface_documents import router as surface_document_router
from backend.mock_business_api.repository import CalculationRepository
from backend.mock_business_api.router import router as calculation_router


ROOT = Path(__file__).resolve().parents[1]


def default_database_path():
    configured = os.environ.get("A2UI_CALCULATION_DB_PATH", "").strip()
    return Path(configured) if configured else ROOT / "data" / "calculations.sqlite3"


def create_demo_app(database_path=None, llm_adapter=None, surface_compiler=None):
    database = Path(database_path) if database_path else default_database_path()

    @asynccontextmanager
    async def lifespan(application):
        database.parent.mkdir(parents=True, exist_ok=True)
        repository = CalculationRepository(database)
        application.state.calculation_repository = repository
        try:
            yield
        finally:
            repository.close()

    application = FastAPI(
        title="A2UI Controlled Qt Composition Demo",
        version="0.1.0",
        lifespan=lifespan,
    )
    application.state.llm_adapter = llm_adapter or adapter_from_environment()
    application.state.surface_compiler = surface_compiler or SurfaceCompiler()
    application.state.effective_catalog = json.loads(
        (ROOT / "shared" / "catalog" / "component-catalog.json").read_text(
            encoding="utf-8"
        )
    )
    application.state.default_surface = load_default_surface()
    application.include_router(composition_router)
    application.include_router(surface_document_router)
    application.include_router(calculation_router)

    @application.get("/health")
    def health():
        return {
            "status": "ok",
            "compositionApi": "/compose",
            "calculationApi": "/api/calculations",
        }

    return application


app = create_demo_app()
