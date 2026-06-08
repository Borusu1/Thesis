from typing import Annotated

import jwt
from fastapi import Depends, HTTPException, status
from fastapi.security import OAuth2PasswordBearer
from sqlalchemy.ext.asyncio import AsyncSession

from app.core.config import get_settings
from app.db.session import get_db_session
from app.models import Device, User
from app.services.devices import get_device_by_id
from app.services.users import get_user_by_id

oauth2_scheme = OAuth2PasswordBearer(tokenUrl="/api/v1/auth/login")
device_oauth2_scheme = OAuth2PasswordBearer(tokenUrl="/api/v1/device/auth/login")


def decode_token(token: str) -> dict[str, object]:
    settings = get_settings()
    return jwt.decode(
        token,
        settings.jwt_secret_key,
        algorithms=[settings.jwt_algorithm],
    )


async def get_current_user(
    token: Annotated[str, Depends(oauth2_scheme)],
    session: Annotated[AsyncSession, Depends(get_db_session)],
) -> User:
    credentials_exception = HTTPException(
        status_code=status.HTTP_401_UNAUTHORIZED,
        detail="Could not validate credentials",
        headers={"WWW-Authenticate": "Bearer"},
    )
    try:
        payload = decode_token(token)
        subject = payload.get("sub")
        token_type = payload.get("token_type")
        if subject is None or token_type != "user":
            raise credentials_exception
        user_id = int(subject)
    except (jwt.InvalidTokenError, ValueError) as error:
        raise credentials_exception from error

    user = await get_user_by_id(session, user_id)
    if user is None or not user.is_active:
        raise credentials_exception

    return user


async def get_current_device(
    token: Annotated[str, Depends(device_oauth2_scheme)],
    session: Annotated[AsyncSession, Depends(get_db_session)],
) -> Device:
    credentials_exception = HTTPException(
        status_code=status.HTTP_401_UNAUTHORIZED,
        detail="Could not validate device credentials",
        headers={"WWW-Authenticate": "Bearer"},
    )
    try:
        payload = decode_token(token)
        subject = payload.get("sub")
        token_type = payload.get("token_type")
        if subject is None or token_type != "device":
            raise credentials_exception
        device_id = int(subject)
    except (jwt.InvalidTokenError, ValueError) as error:
        raise credentials_exception from error

    device = await get_device_by_id(session, device_id)
    if device is None or not device.is_active:
        raise credentials_exception

    return device
