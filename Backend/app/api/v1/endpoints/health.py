from fastapi import APIRouter
from pydantic import BaseModel

from app.core.config import get_settings

router = APIRouter(tags=["health"])


class HealthcheckResponse(BaseModel):
    status: str
    service: str
    environment: str


@router.get("/health", response_model=HealthcheckResponse, summary="Basic service healthcheck")
async def healthcheck() -> HealthcheckResponse:
    settings = get_settings()
    return HealthcheckResponse(
        status="ok",
        service=settings.app_name,
        environment=settings.app_env,
    )

