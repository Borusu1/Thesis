from __future__ import annotations

from datetime import UTC, datetime
from uuid import UUID

from sqlalchemy import desc, select
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.orm import selectinload

from app.models import (
    Device,
    InventoryEvent,
    InventoryEventSource,
    InventoryEventType,
    NFCTag,
    Product,
    TagUsage,
    User,
)


class InventoryError(Exception):
    pass


class ProductNotFoundError(InventoryError):
    pass


class TagUsageNotFoundError(InventoryError):
    pass


class TagAlreadyInUseError(InventoryError):
    pass


class TagUsageClosedError(InventoryError):
    pass


class InvalidMovementQuantityError(InventoryError):
    pass


class TagProvisionError(InventoryError):
    pass


async def get_tag_by_uid(session: AsyncSession, tag_uid: UUID) -> NFCTag | None:
    result = await session.execute(select(NFCTag).where(NFCTag.uid == tag_uid))
    return result.scalar_one_or_none()


async def get_tag_by_chip_uid(session: AsyncSession, chip_uid_hex: str) -> NFCTag | None:
    result = await session.execute(select(NFCTag).where(NFCTag.chip_uid_hex == chip_uid_hex))
    return result.scalar_one_or_none()


async def get_active_usage_for_tag(session: AsyncSession, tag_id: int) -> TagUsage | None:
    result = await session.execute(
        select(TagUsage).where(
            TagUsage.tag_id == tag_id,
            TagUsage.closed_at.is_(None),
        )
    )
    return result.scalar_one_or_none()


async def get_usage_by_id(session: AsyncSession, usage_id: int) -> TagUsage | None:
    result = await session.execute(
        select(TagUsage)
        .options(
            selectinload(TagUsage.product),
            selectinload(TagUsage.tag),
            selectinload(TagUsage.events),
        )
        .where(TagUsage.id == usage_id)
    )
    return result.scalar_one_or_none()


async def get_event_by_client_operation_id(
    session: AsyncSession,
    client_operation_id: str,
) -> InventoryEvent | None:
    result = await session.execute(
        select(InventoryEvent)
        .options(
            selectinload(InventoryEvent.usage).selectinload(TagUsage.product),
            selectinload(InventoryEvent.usage).selectinload(TagUsage.tag),
            selectinload(InventoryEvent.usage).selectinload(TagUsage.events),
        )
        .where(InventoryEvent.client_operation_id == client_operation_id)
    )
    return result.scalar_one_or_none()


async def get_active_usage_by_tag_uid(session: AsyncSession, tag_uid: UUID) -> TagUsage:
    result = await session.execute(
        select(TagUsage)
        .join(TagUsage.tag)
        .options(
            selectinload(TagUsage.product),
            selectinload(TagUsage.tag),
            selectinload(TagUsage.events),
        )
        .where(
            NFCTag.uid == tag_uid,
            TagUsage.closed_at.is_(None),
        )
    )
    usage = result.scalar_one_or_none()
    if usage is None:
        raise TagUsageNotFoundError(f"Active tag usage for {tag_uid} was not found.")
    return usage


async def list_active_tag_usages(session: AsyncSession) -> list[TagUsage]:
    result = await session.execute(
        select(TagUsage)
        .options(
            selectinload(TagUsage.product),
            selectinload(TagUsage.tag),
        )
        .where(TagUsage.closed_at.is_(None))
        .order_by(desc(TagUsage.arrived_at), desc(TagUsage.id))
    )
    return list(result.scalars())


async def list_inventory_events(
    session: AsyncSession,
    *,
    product_id: int | None = None,
    tag_uid: UUID | None = None,
    event_type: InventoryEventType | None = None,
) -> list[InventoryEvent]:
    query = (
        select(InventoryEvent)
        .join(InventoryEvent.usage)
        .join(TagUsage.tag)
        .options(
            selectinload(InventoryEvent.usage).selectinload(TagUsage.tag),
            selectinload(InventoryEvent.actor_user),
            selectinload(InventoryEvent.actor_device),
        )
        .order_by(desc(InventoryEvent.occurred_at), desc(InventoryEvent.id))
    )

    if product_id is not None:
        query = query.where(TagUsage.product_id == product_id)

    if tag_uid is not None:
        query = query.where(NFCTag.uid == tag_uid)

    if event_type is not None:
        query = query.where(InventoryEvent.event_type == event_type)

    result = await session.execute(query)
    return list(result.scalars())


async def get_tag_history(session: AsyncSession, tag_uid: UUID) -> tuple[NFCTag, list[TagUsage]]:
    tag = await get_tag_by_uid(session, tag_uid)
    if tag is None:
        raise TagUsageNotFoundError(f"Tag {tag_uid} was not found.")

    result = await session.execute(
        select(TagUsage)
        .options(
            selectinload(TagUsage.product),
            selectinload(TagUsage.tag),
            selectinload(TagUsage.events),
        )
        .where(TagUsage.tag_id == tag.id)
        .order_by(TagUsage.arrived_at.asc(), TagUsage.id.asc())
    )
    return tag, list(result.scalars())


async def lookup_tag(session: AsyncSession, tag_uid: UUID) -> tuple[NFCTag, TagUsage | None, list[TagUsage]]:
    tag, usages = await get_tag_history(session, tag_uid)
    active_usage = next((usage for usage in usages if usage.closed_at is None), None)
    return tag, active_usage, usages


def apply_tag_metadata(
    tag: NFCTag,
    *,
    chip_uid_hex: str | None = None,
    provisioned_by_device: Device | None = None,
    provisioned_at: datetime | None = None,
) -> None:
    if chip_uid_hex:
        tag.chip_uid_hex = chip_uid_hex
    if provisioned_by_device:
        tag.provisioned_by_device = provisioned_by_device
    if provisioned_at:
        tag.provisioned_at = provisioned_at


async def tag_has_any_usage(session: AsyncSession, tag_id: int) -> bool:
    result = await session.execute(select(TagUsage.id).where(TagUsage.tag_id == tag_id).limit(1))
    return result.scalar_one_or_none() is not None


def build_inventory_event(
    *,
    event_type: InventoryEventType,
    quantity: int,
    occurred_at: datetime,
    note: str | None,
    actor_user: User | None,
    actor_device: Device | None,
    client_operation_id: str | None,
    source: InventoryEventSource,
) -> InventoryEvent:
    return InventoryEvent(
        event_type=event_type,
        quantity=quantity,
        occurred_at=occurred_at,
        note=note,
        actor_user=actor_user,
        actor_device=actor_device,
        client_operation_id=client_operation_id,
        source=source,
    )


async def receive_product(
    session: AsyncSession,
    *,
    product_id: int,
    tag_uid: UUID,
    quantity: int,
    arrived_at: datetime | None = None,
    warehouse_location: str | None = None,
    note: str | None = None,
    actor_user: User | None = None,
    actor_device: Device | None = None,
    client_operation_id: str | None = None,
    source: InventoryEventSource = InventoryEventSource.API,
    chip_uid_hex: str | None = None,
) -> TagUsage:
    if client_operation_id:
        existing_event = await get_event_by_client_operation_id(session, client_operation_id)
        if existing_event is not None:
            return existing_event.usage

    if quantity <= 0:
        raise InvalidMovementQuantityError("Receipt quantity must be greater than zero.")

    product = await session.get(Product, product_id)
    if product is None:
        raise ProductNotFoundError(f"Product {product_id} was not found.")

    normalized_chip_uid = chip_uid_hex.lower() if chip_uid_hex else None
    tag = await get_tag_by_uid(session, tag_uid)
    if source == InventoryEventSource.DEVICE_SYNC:
        if tag is None:
            raise TagProvisionError(f"Tag {tag_uid} is not provisioned in backend.")
        if normalized_chip_uid is not None and tag.chip_uid_hex != normalized_chip_uid:
            raise TagProvisionError("Provision sync required before receipt.")
    else:
        if tag is None:
            tag = NFCTag(uid=tag_uid)
            session.add(tag)
            await session.flush()
        apply_tag_metadata(
            tag,
            chip_uid_hex=normalized_chip_uid,
            provisioned_by_device=actor_device if source != InventoryEventSource.API else None,
        )

    active_usage = await get_active_usage_for_tag(session, tag.id)
    if active_usage is not None:
        raise TagAlreadyInUseError(f"Tag {tag_uid} already has an active assignment.")

    event_time = arrived_at or datetime.now(UTC)
    usage = TagUsage(
        tag=tag,
        product=product,
        product_name_snapshot=product.name,
        quantity_initial=quantity,
        quantity_current=quantity,
        arrived_at=event_time,
        warehouse_location=warehouse_location,
    )
    product.quantity_on_hand += quantity
    usage.events.append(
        build_inventory_event(
            event_type=InventoryEventType.RECEIPT,
            quantity=quantity,
            occurred_at=event_time,
            note=note,
            actor_user=actor_user,
            actor_device=actor_device,
            client_operation_id=client_operation_id,
            source=source,
        )
    )
    session.add(usage)
    await session.flush()
    return usage


async def ship_partial(
    session: AsyncSession,
    *,
    usage_id: int,
    quantity: int,
    occurred_at: datetime | None = None,
    note: str | None = None,
    actor_user: User | None = None,
    actor_device: Device | None = None,
    client_operation_id: str | None = None,
    source: InventoryEventSource = InventoryEventSource.API,
) -> TagUsage:
    if client_operation_id:
        existing_event = await get_event_by_client_operation_id(session, client_operation_id)
        if existing_event is not None:
            return existing_event.usage

    usage = await get_usage_by_id(session, usage_id)
    if usage is None:
        raise TagUsageNotFoundError(f"Tag usage {usage_id} was not found.")
    if usage.closed_at is not None:
        raise TagUsageClosedError("Closed tag usage cannot be shipped again.")
    if quantity <= 0 or quantity >= usage.quantity_current:
        raise InvalidMovementQuantityError(
            "Partial shipment quantity must be greater than zero and less than the remaining quantity."
        )
    if usage.product.quantity_on_hand < quantity:
        raise InvalidMovementQuantityError("Shipment quantity exceeds product stock on hand.")

    event_time = occurred_at or datetime.now(UTC)
    usage.quantity_current -= quantity
    usage.product.quantity_on_hand -= quantity
    usage.events.append(
        build_inventory_event(
            event_type=InventoryEventType.SHIPMENT_PARTIAL,
            quantity=quantity,
            occurred_at=event_time,
            note=note,
            actor_user=actor_user,
            actor_device=actor_device,
            client_operation_id=client_operation_id,
            source=source,
        )
    )
    await session.flush()
    return usage


async def ship_full(
    session: AsyncSession,
    *,
    usage_id: int,
    occurred_at: datetime | None = None,
    note: str | None = None,
    actor_user: User | None = None,
    actor_device: Device | None = None,
    client_operation_id: str | None = None,
    source: InventoryEventSource = InventoryEventSource.API,
) -> TagUsage:
    if client_operation_id:
        existing_event = await get_event_by_client_operation_id(session, client_operation_id)
        if existing_event is not None:
            return existing_event.usage

    usage = await get_usage_by_id(session, usage_id)
    if usage is None:
        raise TagUsageNotFoundError(f"Tag usage {usage_id} was not found.")
    if usage.closed_at is not None or usage.quantity_current == 0:
        raise TagUsageClosedError("Tag usage is already closed.")
    if usage.product.quantity_on_hand < usage.quantity_current:
        raise InvalidMovementQuantityError("Shipment quantity exceeds product stock on hand.")

    event_time = occurred_at or datetime.now(UTC)
    remaining_quantity = usage.quantity_current
    usage.product.quantity_on_hand -= remaining_quantity
    usage.quantity_current = 0
    usage.closed_at = event_time
    usage.events.append(
        build_inventory_event(
            event_type=InventoryEventType.SHIPMENT_FULL,
            quantity=remaining_quantity,
            occurred_at=event_time,
            note=note,
            actor_user=actor_user,
            actor_device=actor_device,
            client_operation_id=client_operation_id,
            source=source,
        )
    )
    await session.flush()
    return usage


async def ship_partial_by_tag_uid(
    session: AsyncSession,
    *,
    tag_uid: UUID,
    quantity: int,
    occurred_at: datetime | None = None,
    note: str | None = None,
    actor_user: User | None = None,
    actor_device: Device | None = None,
    client_operation_id: str | None = None,
    source: InventoryEventSource = InventoryEventSource.API,
) -> TagUsage:
    active_usage = await get_active_usage_by_tag_uid(session, tag_uid)
    return await ship_partial(
        session,
        usage_id=active_usage.id,
        quantity=quantity,
        occurred_at=occurred_at,
        note=note,
        actor_user=actor_user,
        actor_device=actor_device,
        client_operation_id=client_operation_id,
        source=source,
    )


async def ship_full_by_tag_uid(
    session: AsyncSession,
    *,
    tag_uid: UUID,
    occurred_at: datetime | None = None,
    note: str | None = None,
    actor_user: User | None = None,
    actor_device: Device | None = None,
    client_operation_id: str | None = None,
    source: InventoryEventSource = InventoryEventSource.API,
) -> TagUsage:
    active_usage = await get_active_usage_by_tag_uid(session, tag_uid)
    return await ship_full(
        session,
        usage_id=active_usage.id,
        occurred_at=occurred_at,
        note=note,
        actor_user=actor_user,
        actor_device=actor_device,
        client_operation_id=client_operation_id,
        source=source,
    )


async def provision_tag(
    session: AsyncSession,
    *,
    tag_uid: UUID,
    chip_uid_hex: str | None,
    actor_device: Device,
    provisioned_at: datetime | None = None,
) -> tuple[NFCTag, bool]:
    normalized_chip_uid = chip_uid_hex.lower() if chip_uid_hex else None
    target_tag = await get_tag_by_uid(session, tag_uid)
    chip_tag = await get_tag_by_chip_uid(session, normalized_chip_uid) if normalized_chip_uid else None

    if chip_tag is not None and target_tag is not None and chip_tag.id != target_tag.id:
        if await get_active_usage_for_tag(session, chip_tag.id) is not None:
            raise TagProvisionError("Cannot reissue a chip UID that still has an active usage.")
        if await get_active_usage_for_tag(session, target_tag.id) is not None:
            raise TagProvisionError("Cannot reprovision an active tag with a different chip UID.")
        if await tag_has_any_usage(session, target_tag.id):
            raise TagProvisionError("Cannot replace a backend tag that already has usage history.")

        await session.delete(target_tag)
        await session.flush()
        target_tag = None

    if chip_tag is not None:
        reissued = chip_tag.uid != tag_uid
        if chip_tag.uid != tag_uid and await get_active_usage_for_tag(session, chip_tag.id) is not None:
            raise TagProvisionError("Cannot reissue a chip UID that still has an active usage.")

        chip_tag.uid = tag_uid
        apply_tag_metadata(
            chip_tag,
            chip_uid_hex=normalized_chip_uid,
            provisioned_by_device=actor_device,
            provisioned_at=provisioned_at or datetime.now(UTC),
        )
        await session.flush()
        return chip_tag, reissued

    tag = target_tag
    if tag is None:
        tag = NFCTag(uid=tag_uid)
        session.add(tag)
        await session.flush()

    active_usage = await get_active_usage_for_tag(session, tag.id)
    if active_usage is not None and normalized_chip_uid and tag.chip_uid_hex not in {None, normalized_chip_uid}:
        raise TagProvisionError("Cannot reprovision an active tag with a different chip UID.")

    apply_tag_metadata(
        tag,
        chip_uid_hex=normalized_chip_uid,
        provisioned_by_device=actor_device,
        provisioned_at=provisioned_at or datetime.now(UTC),
    )
    await session.flush()
    return tag, False


async def reissue_tag(
    session: AsyncSession,
    *,
    tag_uid: UUID,
    new_chip_uid_hex: str,
    actor_device: Device,
    provisioned_at: datetime | None = None,
) -> NFCTag:
    tag, _ = await provision_tag(
        session,
        tag_uid=tag_uid,
        chip_uid_hex=new_chip_uid_hex,
        actor_device=actor_device,
        provisioned_at=provisioned_at,
    )
    return tag
