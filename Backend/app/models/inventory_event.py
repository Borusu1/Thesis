from __future__ import annotations

from datetime import datetime
from enum import StrEnum

from sqlalchemy import CheckConstraint, DateTime, Enum, ForeignKey, Integer, String, Text
from sqlalchemy.orm import Mapped, mapped_column, relationship

from app.db.base import Base


class InventoryEventType(StrEnum):
    RECEIPT = "receipt"
    SHIPMENT_PARTIAL = "shipment_partial"
    SHIPMENT_FULL = "shipment_full"


class InventoryEventSource(StrEnum):
    API = "api"
    DEVICE = "device"
    DEVICE_SYNC = "device_sync"


class InventoryEvent(Base):
    __tablename__ = "inventory_events"
    __table_args__ = (
        CheckConstraint("quantity > 0", name="ck_inventory_events_quantity_positive"),
    )

    id: Mapped[int] = mapped_column(primary_key=True, autoincrement=True)
    usage_id: Mapped[int] = mapped_column(ForeignKey("tag_usages.id"), nullable=False)
    event_type: Mapped[InventoryEventType] = mapped_column(
        Enum(
            InventoryEventType,
            name="inventory_event_type",
            values_callable=lambda enum_type: [item.value for item in enum_type],
        ),
        nullable=False,
    )
    quantity: Mapped[int] = mapped_column(Integer, nullable=False)
    occurred_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), nullable=False)
    note: Mapped[str | None] = mapped_column(Text, nullable=True)
    actor_user_id: Mapped[int | None] = mapped_column(ForeignKey("users.id"), nullable=True)
    actor_device_id: Mapped[int | None] = mapped_column(ForeignKey("devices.id"), nullable=True)
    client_operation_id: Mapped[str | None] = mapped_column(String(64), unique=True, nullable=True)
    source: Mapped[InventoryEventSource] = mapped_column(
        Enum(
            InventoryEventSource,
            name="inventory_event_source",
            values_callable=lambda enum_type: [item.value for item in enum_type],
        ),
        nullable=False,
        default=InventoryEventSource.API,
        server_default=InventoryEventSource.API.value,
    )

    usage: Mapped["TagUsage"] = relationship(back_populates="events")
    actor_user: Mapped["User | None"] = relationship()
    actor_device: Mapped["Device | None"] = relationship()
