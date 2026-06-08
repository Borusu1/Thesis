import asyncio
import os
from uuid import uuid4

from fastapi.testclient import TestClient
from sqlalchemy import func, select

from app.core.config import get_settings
from app.db.session import AsyncSessionFactory
from app.models import InventoryEventType, Product, User
from app.services.users import seed_demo_user


def test_healthcheck_preflight_allows_configured_web_origin(client: TestClient) -> None:
    response = client.options(
        "/api/v1/health",
        headers={
            "Origin": "http://localhost:19006",
            "Access-Control-Request-Method": "GET",
            "Access-Control-Request-Headers": "Authorization,Content-Type",
        },
    )

    assert response.status_code == 200
    assert response.headers["access-control-allow-origin"] == "http://localhost:19006"


def test_settings_parse_cors_origins_from_comma_separated_env() -> None:
    previous_value = os.environ.get("BACKEND_CORS_ORIGINS")
    os.environ["BACKEND_CORS_ORIGINS"] = "http://localhost:19006,http://127.0.0.1:19006"
    get_settings.cache_clear()

    try:
        settings = get_settings()
        assert settings.backend_cors_origins == [
            "http://localhost:19006",
            "http://127.0.0.1:19006",
        ]
    finally:
        if previous_value is None:
            os.environ.pop("BACKEND_CORS_ORIGINS", None)
        else:
            os.environ["BACKEND_CORS_ORIGINS"] = previous_value
        get_settings.cache_clear()


def test_seed_demo_user_is_idempotent() -> None:
    async def run_test() -> int:
        async with AsyncSessionFactory() as session:
            await seed_demo_user(
                session,
                email="demo@example.com",
                password="demo123",
            )
            await session.commit()

        async with AsyncSessionFactory() as session:
            await seed_demo_user(
                session,
                email="demo@example.com",
                password="demo123",
            )
            await session.commit()

        async with AsyncSessionFactory() as session:
            return int(
                await session.scalar(
                    select(func.count()).select_from(User).where(User.email == "demo@example.com")
                )
            )

    assert asyncio.run(run_test()) == 1


def test_inventory_events_endpoint_returns_global_history(
    client: TestClient,
    auth_headers: dict[str, str],
    test_product: Product,
) -> None:
    tag_uid = str(uuid4())

    client.post(
        "/api/v1/inventory/receipts",
        json={
            "product_id": test_product.id,
            "tag_uid": tag_uid,
            "quantity": 10,
            "arrived_at": "2026-03-24T10:00:00Z",
            "note": "Receipt event",
        },
        headers=auth_headers,
    )
    client.post(
        f"/api/v1/inventory/tags/{tag_uid}/shipments/partial",
        json={"quantity": 4, "occurred_at": "2026-03-24T11:00:00Z", "note": "Partial event"},
        headers=auth_headers,
    )

    response = client.get("/api/v1/inventory/events", headers=auth_headers)

    assert response.status_code == 200
    payload = response.json()
    assert [item["event_type"] for item in payload] == [
        InventoryEventType.SHIPMENT_PARTIAL,
        InventoryEventType.RECEIPT,
    ]
    assert payload[0]["tag_uid"] == tag_uid
    assert payload[0]["product_id"] == test_product.id
    assert payload[0]["product_name_snapshot"] == test_product.name
    assert payload[0]["usage_id"] == payload[1]["usage_id"]


def test_inventory_events_endpoint_supports_filters(
    client: TestClient,
    auth_headers: dict[str, str],
    test_product: Product,
) -> None:
    second_product = client.post(
        "/api/v1/products",
        json={"sku": 1002, "name": "Apricots"},
        headers=auth_headers,
    ).json()
    first_tag_uid = str(uuid4())
    second_tag_uid = str(uuid4())

    client.post(
        "/api/v1/inventory/receipts",
        json={"product_id": test_product.id, "tag_uid": first_tag_uid, "quantity": 9},
        headers=auth_headers,
    )
    client.post(
        "/api/v1/inventory/receipts",
        json={"product_id": second_product["id"], "tag_uid": second_tag_uid, "quantity": 5},
        headers=auth_headers,
    )
    client.post(
        f"/api/v1/inventory/tags/{first_tag_uid}/shipments/full",
        json={"note": "Closed"},
        headers=auth_headers,
    )

    by_product = client.get(
        f"/api/v1/inventory/events?product_id={test_product.id}",
        headers=auth_headers,
    )
    by_tag = client.get(
        f"/api/v1/inventory/events?tag_uid={first_tag_uid}",
        headers=auth_headers,
    )
    by_type = client.get(
        "/api/v1/inventory/events?event_type=shipment_full",
        headers=auth_headers,
    )

    assert by_product.status_code == 200
    assert all(item["product_id"] == test_product.id for item in by_product.json())

    assert by_tag.status_code == 200
    assert {item["tag_uid"] for item in by_tag.json()} == {first_tag_uid}

    assert by_type.status_code == 200
    assert [item["event_type"] for item in by_type.json()] == [InventoryEventType.SHIPMENT_FULL]
