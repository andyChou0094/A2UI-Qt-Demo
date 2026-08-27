"""Standalone local-loopback FastAPI application for calculation records."""

from contextlib import asynccontextmanager

from fastapi import FastAPI

from .repository import CalculationRepository
from .router import router


def create_business_app(database_path):
    @asynccontextmanager
    async def lifespan(application):
        repository = CalculationRepository(database_path)
        application.state.calculation_repository = repository
        try:
            yield
        finally:
            repository.close()

    application = FastAPI(
        title="A2UI Mock Calculation Business API",
        version="0.1.0",
        lifespan=lifespan,
    )
    application.include_router(router)
    return application
