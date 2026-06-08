from __future__ import annotations

from datetime import datetime
from typing import Literal
from uuid import UUID

from pydantic import BaseModel, ConfigDict, Field

from app.schemas.inventory import TagUsageRead
from app.schemas.product import ProductRead
class DeviceLoginRequest(BaseModel):
    device_id: str = Field(min_length=1, max_length=64)


class DeviceLoginResponse(BaseModel):
    access_token: str
    token_type: str = "bearer"
    device_id: str
    device_name: str


class DeviceTagLookupRead(BaseModel):
    tag_uid: UUID
    chip_uid_hex: str | None
    provisioned_at: datetime | None
    active_usage: TagUsageRead | None
    usages: list[TagUsageRead]


class TagProvisionRequest(BaseModel):
    tag_uid: UUID
    chip_uid_hex: str | None = Field(default=None, max_length=64)


class TagProvisionResponse(BaseModel):
    model_config = ConfigDict(from_attributes=True)

    tag_uid: UUID
    chip_uid_hex: str | None
    provisioned_at: datetime | None
    provisioned_by_device_id: int | None
    reissued: bool = False


class ProductRecentRead(BaseModel):
    products: list[ProductRead]


class DeviceSyncOperationReceiptPayload(BaseModel):
    product_id: int
    tag_uid: UUID
    quantity: int = Field(gt=0)
    note: str | None = None
    chip_uid_hex: str | None = Field(default=None, max_length=64)


class DeviceSyncOperationPartialShipmentPayload(BaseModel):
    tag_uid: UUID
    quantity: int = Field(gt=0)
    note: str | None = None


class DeviceSyncOperationFullShipmentPayload(BaseModel):
    tag_uid: UUID
    note: str | None = None


class DeviceSyncOperationBase(BaseModel):
    client_operation_id: str = Field(min_length=1, max_length=64)


class DeviceSyncReceiptOperation(DeviceSyncOperationBase):
    operation_type: Literal["receipt"]
    payload: DeviceSyncOperationReceiptPayload


class DeviceSyncPartialShipmentOperation(DeviceSyncOperationBase):
    operation_type: Literal["shipment_partial"]
    payload: DeviceSyncOperationPartialShipmentPayload


class DeviceSyncFullShipmentOperation(DeviceSyncOperationBase):
    operation_type: Literal["shipment_full"]
    payload: DeviceSyncOperationFullShipmentPayload


DeviceSyncOperation = (
    DeviceSyncReceiptOperation
    | DeviceSyncPartialShipmentOperation
    | DeviceSyncFullShipmentOperation
)


class DeviceSyncRequest(BaseModel):
    operations: list[DeviceSyncOperation]


class DeviceSyncResult(BaseModel):
    client_operation_id: str
    operation_type: str
    status: Literal["applied"]
    usage: TagUsageRead


class DeviceSyncResponse(BaseModel):
    results: list[DeviceSyncResult]
