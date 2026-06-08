import asyncio
import os
from pathlib import Path
from typing import Iterator

import pytest
from fastapi.testclient import TestClient

TEST_DATABASE_PATH = Path(__file__).parent / "test_app.db"
os.environ["APP_ENV"] = "test"
os.environ["DATABASE_URL"] = os.environ.get("TEST_DATABASE_URL", f"sqlite+aiosqlite:///{TEST_DATABASE_PATH}")
os.environ["JWT_SECRET_KEY"] = "test-secret-key-with-safe-length-123"

from app.core.config import get_settings

get_settings.cache_clear()

from app.core.security import hash_password
from app.db.base import Base
from app.db.session import AsyncSessionFactory, engine
from app.main import app
from app.models import Device, Product, User, UserRole


@pytest.fixture(autouse=True)
def reset_database() -> Iterator[None]:
    async def prepare_database() -> None:
        async with engine.begin() as connection:
            await connection.run_sync(Base.metadata.drop_all)
            await connection.run_sync(Base.metadata.create_all)

    asyncio.run(prepare_database())
    yield


@pytest.fixture
def client() -> Iterator[TestClient]:
    with TestClient(app) as test_client:
        yield test_client


@pytest.fixture
def user_credentials() -> dict[str, str]:
    return {
        "email": "user@example.com",
        "password": "strong-password",
    }


@pytest.fixture
def product_data() -> dict[str, str | int]:
    return {
        "sku": 1001,
        "name": "Apples",
        "description": "Fresh apples",
    }


@pytest.fixture
def device_credentials() -> dict[str, str]:
    return {
        "device_id": "esp32s3-aabbccddeeff",
        "name": "Warehouse terminal 1",
    }


@pytest.fixture
def test_user(user_credentials: dict[str, str]) -> User:
    async def create_user() -> User:
        async with AsyncSessionFactory() as session:
            user = User(
                email=user_credentials["email"],
                password_hash=hash_password(user_credentials["password"]),
                role=UserRole.ADMIN,
            )
            session.add(user)
            await session.commit()
            await session.refresh(user)
            return user

    return asyncio.run(create_user())


@pytest.fixture
def test_product(product_data: dict[str, str]) -> Product:
    async def create_product() -> Product:
        async with AsyncSessionFactory() as session:
            product = Product(
                sku=int(product_data["sku"]),
                name=product_data["name"],
                description=product_data["description"],
            )
            session.add(product)
            await session.commit()
            await session.refresh(product)
            return product

    return asyncio.run(create_product())


@pytest.fixture
def test_device(device_credentials: dict[str, str]) -> Device:
    async def create_device() -> Device:
        async with AsyncSessionFactory() as session:
            device = Device(
                device_id=device_credentials["device_id"],
                name=device_credentials["name"],
                secret_hash=None,
                is_active=True,
            )
            session.add(device)
            await session.commit()
            await session.refresh(device)
            return device

    return asyncio.run(create_device())


@pytest.fixture
def auth_headers(
    client: TestClient,
    test_user: User,
    user_credentials: dict[str, str],
) -> dict[str, str]:
    response = client.post(
        "/api/v1/auth/login",
        data={
            "username": user_credentials["email"],
            "password": user_credentials["password"],
        },
    )
    access_token = response.json()["access_token"]
    return {"Authorization": f"Bearer {access_token}"}


@pytest.fixture
def device_auth_headers(
    client: TestClient,
    test_device: Device,
    device_credentials: dict[str, str],
) -> dict[str, str]:
    response = client.post(
        "/api/v1/device/auth/login",
        json={
            "device_id": device_credentials["device_id"],
        },
    )
    access_token = response.json()["access_token"]
    return {"Authorization": f"Bearer {access_token}"}


@pytest.fixture
def device_operator_headers(
    device_auth_headers: dict[str, str],
) -> dict[str, str]:
    return device_auth_headers
