from uuid import uuid4

import pytest
from fastapi.testclient import TestClient

from app.models import Product


@pytest.mark.parametrize(
    ("method", "path", "payload"),
    [
        ("post", "/api/v1/products", {"json": {"sku": 1001, "name": "Apples", "description": "Fresh apples"}}),
        ("get", "/api/v1/products", {}),
        ("get", "/api/v1/products/1", {}),
        (
            "post",
            "/api/v1/inventory/receipts",
            {"json": {"product_id": 1, "tag_uid": str(uuid4()), "quantity": 10}},
        ),
        (
            "post",
            f"/api/v1/inventory/tags/{uuid4()}/shipments/partial",
            {"json": {"quantity": 1}},
        ),
        (
            "post",
            f"/api/v1/inventory/tags/{uuid4()}/shipments/full",
            {"json": {}},
        ),
        ("get", "/api/v1/inventory/tags/active", {}),
        ("get", f"/api/v1/inventory/tags/{uuid4()}/history", {}),
    ],
)
def test_inventory_api_requires_jwt(
    client: TestClient,
    method: str,
    path: str,
    payload: dict[str, object],
) -> None:
    response = getattr(client, method)(path, **payload)

    assert response.status_code == 401


def test_create_list_and_get_products(
    client: TestClient,
    auth_headers: dict[str, str],
) -> None:
    create_response = client.post(
        "/api/v1/products",
        json={"sku": 1001, "name": "Apples", "description": "Fresh apples"},
        headers=auth_headers,
    )

    assert create_response.status_code == 201
    created_product = create_response.json()
    assert created_product["sku"] == 1001
    assert created_product["name"] == "Apples"
    assert created_product["quantity_on_hand"] == 0

    second_response = client.post(
        "/api/v1/products",
        json={"sku": 1002, "name": "Apricots"},
        headers=auth_headers,
    )
    second_product = second_response.json()

    list_response = client.get("/api/v1/products", headers=auth_headers)
    assert list_response.status_code == 200
    product_ids = [item["id"] for item in list_response.json()]
    assert product_ids[:2] == [second_product["id"], created_product["id"]]

    get_response = client.get(f"/api/v1/products/{created_product['id']}", headers=auth_headers)
    assert get_response.status_code == 200
    assert get_response.json()["description"] == "Fresh apples"


def test_get_missing_product_returns_404(
    client: TestClient,
    auth_headers: dict[str, str],
) -> None:
    response = client.get("/api/v1/products/999", headers=auth_headers)

    assert response.status_code == 404


def test_receipt_creates_active_usage_and_updates_stock(
    client: TestClient,
    auth_headers: dict[str, str],
    test_product: Product,
) -> None:
    tag_uid = str(uuid4())

    response = client.post(
        "/api/v1/inventory/receipts",
        json={
            "product_id": test_product.id,
            "tag_uid": tag_uid,
            "quantity": 20,
            "arrived_at": "2026-03-24T10:00:00Z",
            "warehouse_location": "A-01",
            "note": "Initial receipt",
        },
        headers=auth_headers,
    )

    assert response.status_code == 201
    payload = response.json()
    assert payload["tag_uid"] == tag_uid
    assert payload["product_id"] == test_product.id
    assert payload["quantity_initial"] == 20
    assert payload["quantity_current"] == 20
    assert payload["events"][0]["event_type"] == "receipt"
    assert payload["events"][0]["actor_user_id"] is not None
    assert payload["events"][0]["source"] == "api"

    product_response = client.get(f"/api/v1/products/{test_product.id}", headers=auth_headers)
    assert product_response.json()["quantity_on_hand"] == 20


def test_receipt_rejects_tag_with_existing_active_usage(
    client: TestClient,
    auth_headers: dict[str, str],
    test_product: Product,
) -> None:
    tag_uid = str(uuid4())
    payload = {
        "product_id": test_product.id,
        "tag_uid": tag_uid,
        "quantity": 20,
    }

    first_response = client.post("/api/v1/inventory/receipts", json=payload, headers=auth_headers)
    second_response = client.post("/api/v1/inventory/receipts", json=payload, headers=auth_headers)

    assert first_response.status_code == 201
    assert second_response.status_code == 409


def test_partial_shipment_updates_usage_and_stock(
    client: TestClient,
    auth_headers: dict[str, str],
    test_product: Product,
) -> None:
    tag_uid = str(uuid4())
    client.post(
        "/api/v1/inventory/receipts",
        json={"product_id": test_product.id, "tag_uid": tag_uid, "quantity": 20},
        headers=auth_headers,
    )

    response = client.post(
        f"/api/v1/inventory/tags/{tag_uid}/shipments/partial",
        json={"quantity": 5, "occurred_at": "2026-03-24T11:00:00Z", "note": "Picked 5"},
        headers=auth_headers,
    )

    assert response.status_code == 200
    payload = response.json()
    assert payload["quantity_current"] == 15
    assert payload["closed_at"] is None
    assert payload["events"][-1]["event_type"] == "shipment_partial"
    assert payload["events"][-1]["quantity"] == 5

    product_response = client.get(f"/api/v1/products/{test_product.id}", headers=auth_headers)
    assert product_response.json()["quantity_on_hand"] == 15


def test_full_shipment_closes_usage_and_allows_tag_reuse(
    client: TestClient,
    auth_headers: dict[str, str],
    test_product: Product,
) -> None:
    second_product_response = client.post(
        "/api/v1/products",
        json={"sku": 1002, "name": "Apricots"},
        headers=auth_headers,
    )
    second_product_id = second_product_response.json()["id"]
    tag_uid = str(uuid4())

    client.post(
        "/api/v1/inventory/receipts",
        json={"product_id": test_product.id, "tag_uid": tag_uid, "quantity": 12},
        headers=auth_headers,
    )

    full_response = client.post(
        f"/api/v1/inventory/tags/{tag_uid}/shipments/full",
        json={"occurred_at": "2026-03-24T12:00:00Z"},
        headers=auth_headers,
    )

    assert full_response.status_code == 200
    full_payload = full_response.json()
    assert full_payload["quantity_current"] == 0
    assert full_payload["closed_at"] is not None
    assert full_payload["events"][-1]["event_type"] == "shipment_full"

    reuse_response = client.post(
        "/api/v1/inventory/receipts",
        json={"product_id": second_product_id, "tag_uid": tag_uid, "quantity": 7},
        headers=auth_headers,
    )

    assert reuse_response.status_code == 201
    assert reuse_response.json()["quantity_current"] == 7


def test_active_tags_endpoint_returns_only_open_usages(
    client: TestClient,
    auth_headers: dict[str, str],
    test_product: Product,
) -> None:
    second_product_response = client.post(
        "/api/v1/products",
        json={"sku": 1002, "name": "Apricots"},
        headers=auth_headers,
    )
    second_product_id = second_product_response.json()["id"]
    closed_tag_uid = str(uuid4())
    active_tag_uid = str(uuid4())

    client.post(
        "/api/v1/inventory/receipts",
        json={"product_id": test_product.id, "tag_uid": closed_tag_uid, "quantity": 10},
        headers=auth_headers,
    )
    client.post(
        f"/api/v1/inventory/tags/{closed_tag_uid}/shipments/full",
        json={},
        headers=auth_headers,
    )
    client.post(
        "/api/v1/inventory/receipts",
        json={"product_id": second_product_id, "tag_uid": active_tag_uid, "quantity": 5},
        headers=auth_headers,
    )

    response = client.get("/api/v1/inventory/tags/active", headers=auth_headers)

    assert response.status_code == 200
    payload = response.json()
    assert len(payload) == 1
    assert payload[0]["tag_uid"] == active_tag_uid
    assert payload[0]["quantity_current"] == 5


def test_tag_history_returns_all_usages_and_events_in_order(
    client: TestClient,
    auth_headers: dict[str, str],
    test_product: Product,
) -> None:
    second_product_response = client.post(
        "/api/v1/products",
        json={"sku": 1002, "name": "Apricots"},
        headers=auth_headers,
    )
    second_product_id = second_product_response.json()["id"]
    tag_uid = str(uuid4())

    client.post(
        "/api/v1/inventory/receipts",
        json={
            "product_id": test_product.id,
            "tag_uid": tag_uid,
            "quantity": 10,
            "arrived_at": "2026-03-24T10:00:00Z",
        },
        headers=auth_headers,
    )
    client.post(
        f"/api/v1/inventory/tags/{tag_uid}/shipments/partial",
        json={"quantity": 3, "occurred_at": "2026-03-24T11:00:00Z"},
        headers=auth_headers,
    )
    client.post(
        f"/api/v1/inventory/tags/{tag_uid}/shipments/full",
        json={"occurred_at": "2026-03-24T12:00:00Z"},
        headers=auth_headers,
    )
    client.post(
        "/api/v1/inventory/receipts",
        json={
            "product_id": second_product_id,
            "tag_uid": tag_uid,
            "quantity": 7,
            "arrived_at": "2026-03-24T13:00:00Z",
        },
        headers=auth_headers,
    )

    response = client.get(f"/api/v1/inventory/tags/{tag_uid}/history", headers=auth_headers)

    assert response.status_code == 200
    payload = response.json()
    assert payload["tag_uid"] == tag_uid
    assert len(payload["usages"]) == 2
    assert [event["event_type"] for event in payload["usages"][0]["events"]] == [
        "receipt",
        "shipment_partial",
        "shipment_full",
    ]
    assert [event["event_type"] for event in payload["usages"][1]["events"]] == ["receipt"]
    assert payload["usages"][0]["product_id"] == test_product.id
    assert payload["usages"][1]["product_id"] == second_product_id
