from __future__ import annotations

from datetime import UTC, datetime

from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from app.core.security import hash_password
from app.models import Device


async def get_device_by_id(session: AsyncSession, device_id: int) -> Device | None:
    return await session.get(Device, device_id)


async def get_device_by_device_id(session: AsyncSession, device_id: str) -> Device | None:
    result = await session.execute(select(Device).where(Device.device_id == device_id))
    return result.scalar_one_or_none()


async def authenticate_device(session: AsyncSession, device_id: str) -> Device | None:
    device = await get_device_by_device_id(session, device_id)
    if device is None:
        device = Device(
            device_id=device_id,
            name=f"ESP32 {device_id}",
            secret_hash=None,
            is_active=True,
        )
        session.add(device)
        await session.flush()
    elif not device.is_active:
        return None

    device.last_seen_at = datetime.now(UTC)
    await session.flush()
    return device


async def create_device(
    session: AsyncSession,
    *,
    device_id: str,
    name: str,
    secret: str | None = None,
    is_active: bool = True,
) -> Device:
    device = Device(
        device_id=device_id,
        name=name,
        secret_hash=hash_password(secret) if secret else None,
        is_active=is_active,
    )
    session.add(device)
    await session.flush()
    return device
