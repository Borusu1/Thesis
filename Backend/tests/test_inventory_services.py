from __future__ import annotations

from datetime import UTC, datetime
from uuid import uuid4

import pytest
from sqlalchemy import select
from sqlalchemy.orm import selectinload

from app.db.session import AsyncSessionFactory
from app.models import InventoryEventType, Product, TagUsage
from app.services.inventory import InvalidMovementQuantityError, receive_product, ship_full, ship_partial


@pytest.mark.anyio
async def test_receive_product_creates_active_usage_and_updates_stock(test_product: Product) -> None:
    tag_uid = uuid4()
    arrival_time = datetime(2026, 3, 24, 10, 0, tzinfo=UTC)

    async with AsyncSessionFactory() as session:
        usage = await receive_product(
            session,
            product_id=test_product.id,
            tag_uid=tag_uid,
            quantity=20,
            arrived_at=arrival_time,
            warehouse_location="A-01",
        )
        await session.commit()
        result = await session.execute(
            select(TagUsage)
            .options(
                selectinload(TagUsage.tag),
                selectinload(TagUsage.product),
                selectinload(TagUsage.events),
            )
            .where(TagUsage.id == usage.id)
        )
        reloaded_usage = result.scalar_one()

        assert reloaded_usage.tag.uid == tag_uid
        assert reloaded_usage.quantity_initial == 20
        assert reloaded_usage.quantity_current == 20
        assert reloaded_usage.closed_at is None
        assert reloaded_usage.product.quantity_on_hand == 20
        assert reloaded_usage.events[0].event_type == InventoryEventType.RECEIPT


@pytest.mark.anyio
async def test_partial_shipment_reduces_usage_quantity_and_stock(test_product: Product) -> None:
    tag_uid = uuid4()

    async with AsyncSessionFactory() as session:
        usage = await receive_product(session, product_id=test_product.id, tag_uid=tag_uid, quantity=20)
        await ship_partial(session, usage_id=usage.id, quantity=5)
        await session.commit()

        reloaded_usage = await session.get(TagUsage, usage.id)
        reloaded_product = await session.get(Product, test_product.id)

        assert reloaded_usage.quantity_current == 15
        assert reloaded_usage.closed_at is None
        assert reloaded_product.quantity_on_hand == 15


@pytest.mark.anyio
async def test_full_shipment_closes_usage_and_frees_tag_for_reuse(test_product: Product) -> None:
    first_tag_uid = uuid4()
    second_product = Product(sku=1002, name="Apricots")

    async with AsyncSessionFactory() as session:
        session.add(second_product)
        await session.flush()

        first_usage = await receive_product(session, product_id=test_product.id, tag_uid=first_tag_uid, quantity=12)
        await ship_full(session, usage_id=first_usage.id)
        second_usage = await receive_product(
            session,
            product_id=second_product.id,
            tag_uid=first_tag_uid,
            quantity=7,
        )
        await session.commit()

        await session.refresh(first_usage)
        await session.refresh(second_usage)

        assert first_usage.closed_at is not None
        assert first_usage.quantity_current == 0
        assert second_usage.tag_id == first_usage.tag_id
        assert second_usage.closed_at is None


@pytest.mark.anyio
async def test_partial_shipment_rejects_quantity_greater_than_remaining(test_product: Product) -> None:
    tag_uid = uuid4()

    async with AsyncSessionFactory() as session:
        usage = await receive_product(session, product_id=test_product.id, tag_uid=tag_uid, quantity=10)

        with pytest.raises(InvalidMovementQuantityError):
            await ship_partial(session, usage_id=usage.id, quantity=10)
