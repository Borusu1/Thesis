from fastapi.testclient import TestClient


def test_login_returns_access_token(
    client: TestClient,
    test_user: object,
    user_credentials: dict[str, str],
) -> None:
    response = client.post(
        "/api/v1/auth/login",
        data={
            "username": user_credentials["email"],
            "password": user_credentials["password"],
        },
    )

    assert response.status_code == 200
    payload = response.json()
    assert payload["token_type"] == "bearer"
    assert isinstance(payload["access_token"], str)
    assert payload["access_token"]


def test_login_rejects_invalid_password(
    client: TestClient,
    test_user: object,
    user_credentials: dict[str, str],
) -> None:
    response = client.post(
        "/api/v1/auth/login",
        data={
            "username": user_credentials["email"],
            "password": "wrong-password",
        },
    )

    assert response.status_code == 401
    assert response.json() == {"detail": "Incorrect email or password"}


def test_protected_route_requires_token(client: TestClient) -> None:
    response = client.get("/api/v1/auth/me")

    assert response.status_code == 401


def test_protected_route_returns_current_user(
    client: TestClient,
    test_user: object,
    user_credentials: dict[str, str],
) -> None:
    login_response = client.post(
        "/api/v1/auth/login",
        data={
            "username": user_credentials["email"],
            "password": user_credentials["password"],
        },
    )
    access_token = login_response.json()["access_token"]

    response = client.get(
        "/api/v1/auth/me",
        headers={"Authorization": f"Bearer {access_token}"},
    )

    assert response.status_code == 200
    assert response.json()["email"] == user_credentials["email"]
    assert response.json()["role"] == "admin"
    assert response.json()["is_active"] is True
