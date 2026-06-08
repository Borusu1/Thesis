from __future__ import annotations

from datetime import datetime

from sqlalchemy import CheckConstraint, DateTime, ForeignKey, Index, Integer, String, text
from sqlalchemy.orm import Mapped, mapped_column, relationship

from app.db.base import Base


class TagUsage(Base):
    __tablename__ = "tag_usages"
    __table_args__ = (
        CheckConstraint("quantity_initial > 0", name="ck_tag_usages_quantity_initial_positive"),
        CheckConstraint("quantity_current >= 0", name="ck_tag_usages_quantity_current_non_negative"),
        CheckConstraint(
            "quantity_current <= quantity_initial",
            name="ck_tag_usages_quantity_current_lte_initial",
        ),
        Index(
            "uq_tag_usages_active_tag",
            "tag_id",
            unique=True,
            postgresql_where=text("closed_at IS NULL"),
            sqlite_where=text("closed_at IS NULL"),
        ),
    )

    id: Mapped[int] = mapped_column(primary_key=True, autoincrement=True)
    tag_id: Mapped[int] = mapped_column(ForeignKey("nfc_tags.id"), nullable=False)
    product_id: Mapped[int] = mapped_column(ForeignKey("products.id"), nullable=False)
    product_name_snapshot: Mapped[str] = mapped_column(String(255), nullable=False)
    quantity_initial: Mapped[int] = mapped_column(Integer, nullable=False)
    quantity_current: Mapped[int] = mapped_column(Integer, nullable=False)
    arrived_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), nullable=False)
    warehouse_location: Mapped[str | None] = mapped_column(String(255), nullable=True)
    closed_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)

    product: Mapped["Product"] = relationship(back_populates="tag_usages")
    tag: Mapped["NFCTag"] = relationship(back_populates="tag_usages")
    events: Mapped[list["InventoryEvent"]] = relationship(
        back_populates="usage",
        cascade="all, delete-orphan",
        order_by="InventoryEvent.occurred_at",
    )
