from datetime import datetime
from uuid import UUID

from pydantic import AliasPath, BaseModel, ConfigDict, Field

from app.models import InventoryEventSource, InventoryEventType


class ReceiptCreate(BaseModel):
    product_id: int
    tag_uid: UUID
    quantity: int = Field(gt=0)
    arrived_at: datetime | None = None
    warehouse_location: str | None = Field(default=None, max_length=255)
    note: str | None = None


class PartialShipmentCreate(BaseModel):
    quantity: int = Field(gt=0)
    occurred_at: datetime | None = None
    note: str | None = None


class FullShipmentCreate(BaseModel):
    occurred_at: datetime | None = None
    note: str | None = None


class InventoryEventRead(BaseModel):
    model_config = ConfigDict(from_attributes=True)

    id: int
    event_type: InventoryEventType
    quantity: int
    occurred_at: datetime
    note: str | None
    actor_user_id: int | None
    actor_device_id: int | None
    client_operation_id: str | None
    source: InventoryEventSource


class InventoryEventListItemRead(BaseModel):
    model_config = ConfigDict(from_attributes=True)

    id: int
    usage_id: int
    product_id: int = Field(validation_alias=AliasPath("usage", "product_id"))
    product_name_snapshot: str = Field(validation_alias=AliasPath("usage", "product_name_snapshot"))
    tag_uid: UUID = Field(validation_alias=AliasPath("usage", "tag", "uid"))
    event_type: InventoryEventType
    quantity: int
    occurred_at: datetime
    note: str | None
    actor_user_id: int | None
    actor_device_id: int | None
    source: InventoryEventSource


class TagUsageRead(BaseModel):
    model_config = ConfigDict(from_attributes=True)

    id: int
    tag_uid: UUID = Field(validation_alias=AliasPath("tag", "uid"))
    product_id: int
    product_name_snapshot: str
    quantity_initial: int
    quantity_current: int
    arrived_at: datetime
    warehouse_location: str | None
    closed_at: datetime | None
    events: list[InventoryEventRead] = Field(default_factory=list)


class ActiveTagRead(BaseModel):
    model_config = ConfigDict(from_attributes=True)

    id: int
    tag_uid: UUID = Field(validation_alias=AliasPath("tag", "uid"))
    product_id: int
    product_name_snapshot: str
    quantity_initial: int
    quantity_current: int
    arrived_at: datetime
    warehouse_location: str | None


class TagHistoryRead(BaseModel):
    tag_uid: UUID
    usages: list[TagUsageRead]
