from uuid import uuid4

from fastapi.testclient import TestClient

from app.models import Product


def test_device_login_returns_access_token(
    client: TestClient,
    test_device: object,
    device_credentials: dict[str, str],
) -> None:
    response = client.post(
        "/api/v1/device/auth/login",
        json={
            "device_id": device_credentials["device_id"],
        },
    )

    assert response.status_code == 200
    payload = response.json()
    assert payload["device_id"] == device_credentials["device_id"]
    assert payload["device_name"] == device_credentials["name"]
    assert payload["access_token"]


def test_device_endpoints_are_device_only(
    client: TestClient,
    device_auth_headers: dict[str, str],
) -> None:
    response = client.post(
        "/api/v1/device/tags/provision",
        json={"tag_uid": str(uuid4()), "chip_uid_hex": "04a1b2c3d4"},
        headers=device_auth_headers,
    )

    assert response.status_code == 200


def test_device_products_recent_and_search_by_sku(
    client: TestClient,
    device_auth_headers: dict[str, str],
    auth_headers: dict[str, str],
) -> None:
    client.post("/api/v1/products", json={"sku": 1001, "name": "Apples"}, headers=auth_headers)
    client.post("/api/v1/products", json={"sku": 1002, "name": "Apricots"}, headers=auth_headers)

    recent_response = client.get("/api/v1/device/products/recent", headers=device_auth_headers)
    assert recent_response.status_code == 200
    assert [product["sku"] for product in recent_response.json()["products"]] == [1002, 1001]

    search_response = client.get("/api/v1/device/products/search?sku=1001", headers=device_auth_headers)
    assert search_response.status_code == 200
    assert search_response.json()["name"] == "Apples"


def test_device_can_provision_and_lookup_tag(
    client: TestClient,
    device_operator_headers: dict[str, str],
) -> None:
    tag_uid = str(uuid4())

    provision_response = client.post(
        "/api/v1/device/tags/provision",
        json={"tag_uid": tag_uid, "chip_uid_hex": "04a1b2c3d4"},
        headers=device_operator_headers,
    )

    assert provision_response.status_code == 200
    assert provision_response.json()["chip_uid_hex"] == "04a1b2c3d4"

    lookup_response = client.get(
        f"/api/v1/device/tags/{tag_uid}/lookup",
        headers=device_operator_headers,
    )

    assert lookup_response.status_code == 200
    payload = lookup_response.json()
    assert payload["tag_uid"] == tag_uid
    assert payload["chip_uid_hex"] == "04a1b2c3d4"
    assert payload["active_usage"] is None


def test_device_provision_auto_reissues_same_chip_uid(
    client: TestClient,
    device_operator_headers: dict[str, str],
) -> None:
    original_tag_uid = str(uuid4())
    reissued_tag_uid = str(uuid4())

    first_response = client.post(
        "/api/v1/device/tags/provision",
        json={"tag_uid": original_tag_uid, "chip_uid_hex": "04a1b2c3d4"},
        headers=device_operator_headers,
    )
    second_response = client.post(
        "/api/v1/device/tags/provision",
        json={"tag_uid": reissued_tag_uid, "chip_uid_hex": "04a1b2c3d4"},
        headers=device_operator_headers,
    )

    assert first_response.status_code == 200
    assert first_response.json()["reissued"] is False
    assert second_response.status_code == 200
    assert second_response.json()["reissued"] is True
    assert second_response.json()["tag_uid"] == reissued_tag_uid

    old_lookup = client.get(f"/api/v1/device/tags/{original_tag_uid}/lookup", headers=device_operator_headers)
    new_lookup = client.get(f"/api/v1/device/tags/{reissued_tag_uid}/lookup", headers=device_operator_headers)

    assert old_lookup.status_code == 404
    assert new_lookup.status_code == 200
    assert new_lookup.json()["chip_uid_hex"] == "04a1b2c3d4"


def test_device_sync_receipt_is_idempotent(
    client: TestClient,
    device_operator_headers: dict[str, str],
    test_product: Product,
) -> None:
    tag_uid = str(uuid4())
    provision_response = client.post(
        "/api/v1/device/tags/provision",
        json={"tag_uid": tag_uid, "chip_uid_hex": "04deadbeef"},
        headers=device_operator_headers,
    )
    assert provision_response.status_code == 200

    payload = {
        "operations": [
            {
                "client_operation_id": "op-1",
                "operation_type": "receipt",
                "payload": {
                    "product_id": test_product.id,
                    "tag_uid": tag_uid,
                    "quantity": 5,
                    "chip_uid_hex": "04deadbeef",
                },
            }
        ]
    }

    first_response = client.post(
        "/api/v1/device/operations/sync",
        json=payload,
        headers=device_operator_headers,
    )
    second_response = client.post(
        "/api/v1/device/operations/sync",
        json=payload,
        headers=device_operator_headers,
    )

    assert first_response.status_code == 200
    assert second_response.status_code == 200
    assert first_response.json()["results"][0]["usage"]["id"] == second_response.json()["results"][0]["usage"]["id"]

    history_response = client.get(f"/api/v1/inventory/tags/{tag_uid}/history", headers=device_operator_headers)
    assert history_response.status_code == 401

    lookup_response = client.get(
        f"/api/v1/device/tags/{tag_uid}/lookup",
        headers=device_operator_headers,
    )
    assert lookup_response.status_code == 200
    usages = lookup_response.json()["usages"]
    assert len(usages) == 1
    assert len(usages[0]["events"]) == 1
    assert usages[0]["events"][0]["client_operation_id"] == "op-1"
    assert usages[0]["events"][0]["source"] == "device_sync"


def test_device_sync_receipt_requires_provisioned_tag(
    client: TestClient,
    device_operator_headers: dict[str, str],
    test_product: Product,
) -> None:
    payload = {
        "operations": [
            {
                "client_operation_id": "op-missing-provision",
                "operation_type": "receipt",
                "payload": {
                    "product_id": test_product.id,
                    "tag_uid": str(uuid4()),
                    "quantity": 1,
                    "chip_uid_hex": "04feedface",
                },
            }
        ]
    }

    response = client.post("/api/v1/device/operations/sync", json=payload, headers=device_operator_headers)

    assert response.status_code == 409
    assert "provisioned" in response.json()["detail"].lower()
