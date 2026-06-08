from datetime import datetime

from pydantic import BaseModel, ConfigDict, Field


class ProductCreate(BaseModel):
    sku: int = Field(gt=0)
    name: str = Field(min_length=1, max_length=255)
    description: str | None = None


class ProductRead(BaseModel):
    model_config = ConfigDict(from_attributes=True)

    id: int
    sku: int
    name: str
    description: str | None
    quantity_on_hand: int
    created_at: datetime
