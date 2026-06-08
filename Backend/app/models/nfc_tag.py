from __future__ import annotations

from datetime import datetime
from uuid import UUID

from sqlalchemy import DateTime, ForeignKey, String, Uuid, func
from sqlalchemy.orm import Mapped, mapped_column, relationship

from app.db.base import Base


class NFCTag(Base):
    __tablename__ = "nfc_tags"

    id: Mapped[int] = mapped_column(primary_key=True, autoincrement=True)
    uid: Mapped[UUID] = mapped_column(Uuid, unique=True, index=True, nullable=False)
    chip_uid_hex: Mapped[str | None] = mapped_column(String(64), nullable=True, unique=True)
    provisioned_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)
    provisioned_by_device_id: Mapped[int | None] = mapped_column(
        ForeignKey("devices.id"),
        nullable=True,
    )
    created_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True),
        server_default=func.now(),
        nullable=False,
    )

    provisioned_by_device: Mapped["Device | None"] = relationship(back_populates="provisioned_tags")
    tag_usages: Mapped[list["TagUsage"]] = relationship(back_populates="tag")
