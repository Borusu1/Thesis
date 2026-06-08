from fastapi import FastAPI
from fastapi.testclient import TestClient

from app.core.config import get_settings
from app.main import app


def test_app_is_created() -> None:
    assert isinstance(app, FastAPI)
    assert app.title == get_settings().app_name


def test_healthcheck_returns_expected_payload(client: TestClient) -> None:
    response = client.get("/api/v1/health")

    assert response.status_code == 200
    assert response.json() == {
        "status": "ok",
        "service": "Warehouse NFC Backend",
        "environment": get_settings().app_env,
    }
