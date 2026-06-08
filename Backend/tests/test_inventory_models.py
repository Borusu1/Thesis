from __future__ import annotations

from datetime import UTC, datetime
from uuid import uuid4

import pytest
from sqlalchemy.exc import IntegrityError
from sqlalchemy.dialects import postgresql

from app.db.session import AsyncSessionFactory
from app.models import InventoryEvent, InventoryEventType, NFCTag, Product, TagUsage


@pytest.mark.anyio
async def test_product_rejects_negative_quantity() -> None:
    async with AsyncSessionFactory() as session:
        session.add(Product(sku=1001, name="Apples", quantity_on_hand=-1))

        with pytest.raises(IntegrityError):
            await session.commit()


@pytest.mark.anyio
async def test_nfc_tag_uid_must_be_unique() -> None:
    tag_uid = uuid4()

    async with AsyncSessionFactory() as session:
        session.add_all([NFCTag(uid=tag_uid), NFCTag(uid=tag_uid)])

        with pytest.raises(IntegrityError):
            await session.commit()


@pytest.mark.anyio
async def test_only_one_active_usage_is_allowed_for_a_tag() -> None:
    arrived_at = datetime(2026, 3, 24, 10, 0, tzinfo=UTC)
    tag = NFCTag(uid=uuid4())
    product_one = Product(sku=1001, name="Apples")
    product_two = Product(sku=1002, name="Apricots")

    async with AsyncSessionFactory() as session:
        session.add_all([tag, product_one, product_two])
        await session.flush()

        session.add(
            TagUsage(
                tag_id=tag.id,
                product_id=product_one.id,
                product_name_snapshot=product_one.name,
                quantity_initial=10,
                quantity_current=10,
                arrived_at=arrived_at,
            )
        )
        await session.flush()

        session.add(
            TagUsage(
                tag_id=tag.id,
                product_id=product_two.id,
                product_name_snapshot=product_two.name,
                quantity_initial=5,
                quantity_current=5,
                arrived_at=arrived_at,
            )
        )

        with pytest.raises(IntegrityError):
            await session.commit()


@pytest.mark.anyio
async def test_closed_usage_frees_tag_for_reuse() -> None:
    arrived_at = datetime(2026, 3, 24, 10, 0, tzinfo=UTC)
    tag = NFCTag(uid=uuid4())
    product_one = Product(sku=1001, name="Apples")
    product_two = Product(sku=1002, name="Apricots")

    async with AsyncSessionFactory() as session:
        session.add_all([tag, product_one, product_two])
        await session.flush()

        first_usage = TagUsage(
            tag_id=tag.id,
            product_id=product_one.id,
            product_name_snapshot=product_one.name,
            quantity_initial=10,
            quantity_current=0,
            arrived_at=arrived_at,
            closed_at=arrived_at,
        )
        second_usage = TagUsage(
            tag_id=tag.id,
            product_id=product_two.id,
            product_name_snapshot=product_two.name,
            quantity_initial=5,
            quantity_current=5,
            arrived_at=arrived_at,
        )

        session.add_all([first_usage, second_usage])
        await session.commit()

        assert second_usage.id is not None


@pytest.mark.anyio
async def test_usage_rejects_quantity_greater_than_initial() -> None:
    arrived_at = datetime(2026, 3, 24, 10, 0, tzinfo=UTC)
    tag = NFCTag(uid=uuid4())
    product = Product(sku=1001, name="Apples")

    async with AsyncSessionFactory() as session:
        session.add_all([tag, product])
        await session.flush()

        session.add(
            TagUsage(
                tag_id=tag.id,
                product_id=product.id,
                product_name_snapshot=product.name,
                quantity_initial=10,
                quantity_current=11,
                arrived_at=arrived_at,
            )
        )

        with pytest.raises(IntegrityError):
            await session.commit()


@pytest.mark.anyio
async def test_inventory_event_quantity_must_be_positive() -> None:
    arrived_at = datetime(2026, 3, 24, 10, 0, tzinfo=UTC)
    tag = NFCTag(uid=uuid4())
    product = Product(sku=1001, name="Apples")

    async with AsyncSessionFactory() as session:
        session.add_all([tag, product])
        await session.flush()

        usage = TagUsage(
            tag_id=tag.id,
            product_id=product.id,
            product_name_snapshot=product.name,
            quantity_initial=10,
            quantity_current=10,
            arrived_at=arrived_at,
        )
        session.add(usage)
        await session.flush()

        session.add(
            InventoryEvent(
                usage_id=usage.id,
                event_type=InventoryEventType.RECEIPT,
                quantity=0,
                occurred_at=arrived_at,
            )
        )

        with pytest.raises(IntegrityError):
            await session.commit()


def test_inventory_event_uses_enum_values_for_postgresql_binding() -> None:
    dialect = postgresql.dialect()
    processor = InventoryEvent.__table__.c.event_type.type.bind_processor(dialect)

    assert processor is not None
    assert processor(InventoryEventType.RECEIPT) == "receipt"
