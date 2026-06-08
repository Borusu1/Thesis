from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from app.core.security import hash_password, verify_password
from app.models.user import User, UserRole


async def get_user_by_email(session: AsyncSession, email: str) -> User | None:
    result = await session.execute(select(User).where(User.email == email))
    return result.scalar_one_or_none()


async def get_user_by_id(session: AsyncSession, user_id: int) -> User | None:
    return await session.get(User, user_id)


async def authenticate_user(session: AsyncSession, email: str, password: str) -> User | None:
    user = await get_user_by_email(session, email)
    if user is None:
        return None

    if not verify_password(password, user.password_hash):
        return None

    return user


async def authenticate_operator_by_pin(session: AsyncSession, pin: str) -> User | None:
    result = await session.execute(select(User).where(User.pin_hash.is_not(None)))
    for user in result.scalars():
        if verify_password(pin, user.pin_hash or "") and user.is_active:
            return user
    return None


async def authenticate_operator_by_badge_uid(session: AsyncSession, badge_uid: str) -> User | None:
    result = await session.execute(select(User).where(User.badge_uid == badge_uid))
    user = result.scalar_one_or_none()
    if user is not None:
        return user if user.is_active else None

    unassigned_result = await session.execute(
        select(User).where(User.is_active.is_(True), User.badge_uid.is_(None)).order_by(User.id.asc())
    )
    unassigned_users = list(unassigned_result.scalars())
    if len(unassigned_users) != 1:
        return None

    user = unassigned_users[0]
    user.badge_uid = badge_uid
    await session.flush()
    return user


async def seed_demo_user(
    session: AsyncSession,
    *,
    email: str,
    password: str,
    pin: str = "1234",
    badge_uid: str | None = None,
    role: UserRole = UserRole.ADMIN,
) -> User:
    existing_user = await get_user_by_email(session, email)
    if existing_user is not None:
        changed = False
        if not existing_user.pin_hash:
            existing_user.pin_hash = hash_password(pin)
            changed = True
        if existing_user.role != role:
            existing_user.role = role
            changed = True
        if not existing_user.is_active:
            existing_user.is_active = True
            changed = True
        if badge_uid and existing_user.badge_uid != badge_uid:
            existing_user.badge_uid = badge_uid
            changed = True
        if changed:
            await session.flush()
        return existing_user

    user = User(
        email=email,
        password_hash=hash_password(password),
        pin_hash=hash_password(pin),
        badge_uid=badge_uid,
        role=role,
        is_active=True,
    )
    session.add(user)
    await session.flush()
    return user
