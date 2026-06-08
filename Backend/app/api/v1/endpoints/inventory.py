from typing import Annotated
from uuid import UUID

from fastapi import APIRouter, Depends, HTTPException, status
from sqlalchemy.ext.asyncio import AsyncSession

from app.api.dependencies.auth import get_current_user
from app.db.session import get_db_session
from app.models import InventoryEventSource, InventoryEventType, User
from app.schemas import (
    ActiveTagRead,
    FullShipmentCreate,
    InventoryEventListItemRead,
    PartialShipmentCreate,
    ReceiptCreate,
    TagHistoryRead,
    TagUsageRead,
)
from app.services import (
    InvalidMovementQuantityError,
    ProductNotFoundError,
    TagAlreadyInUseError,
    TagProvisionError,
    TagUsageClosedError,
    TagUsageNotFoundError,
    get_tag_history,
    list_inventory_events,
    list_active_tag_usages,
    receive_product,
    ship_full_by_tag_uid,
    ship_partial_by_tag_uid,
)
from app.services.inventory import get_usage_by_id

router = APIRouter(
    prefix="/inventory",
    tags=["inventory"],
    dependencies=[Depends(get_current_user)],
)


def raise_inventory_http_error(error: Exception) -> None:
    if isinstance(error, (ProductNotFoundError, TagUsageNotFoundError)):
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail=str(error)) from error

    if isinstance(error, (TagAlreadyInUseError, TagUsageClosedError, InvalidMovementQuantityError, TagProvisionError)):
        raise HTTPException(status_code=status.HTTP_409_CONFLICT, detail=str(error)) from error

    raise error


@router.post("/receipts", response_model=TagUsageRead, status_code=status.HTTP_201_CREATED)
async def create_receipt(
    payload: ReceiptCreate,
    session: Annotated[AsyncSession, Depends(get_db_session)],
    current_user: Annotated[User, Depends(get_current_user)],
) -> TagUsageRead:
    try:
        usage = await receive_product(
            session,
            product_id=payload.product_id,
            tag_uid=payload.tag_uid,
            quantity=payload.quantity,
            arrived_at=payload.arrived_at,
            warehouse_location=payload.warehouse_location,
            note=payload.note,
            actor_user=current_user,
            source=InventoryEventSource.API,
        )
        await session.commit()
        reloaded_usage = await get_usage_by_id(session, usage.id)
    except (
        ProductNotFoundError,
        TagUsageNotFoundError,
        TagAlreadyInUseError,
        TagUsageClosedError,
        InvalidMovementQuantityError,
        TagProvisionError,
    ) as error:
        raise_inventory_http_error(error)

    return TagUsageRead.model_validate(reloaded_usage)


@router.get("/tags/active", response_model=list[ActiveTagRead])
async def list_active_tags(
    session: Annotated[AsyncSession, Depends(get_db_session)],
) -> list[ActiveTagRead]:
    tag_usages = await list_active_tag_usages(session)
    return [ActiveTagRead.model_validate(tag_usage) for tag_usage in tag_usages]


@router.get("/events", response_model=list[InventoryEventListItemRead])
async def get_inventory_events(
    session: Annotated[AsyncSession, Depends(get_db_session)],
    product_id: int | None = None,
    tag_uid: UUID | None = None,
    event_type: InventoryEventType | None = None,
) -> list[InventoryEventListItemRead]:
    events = await list_inventory_events(
        session,
        product_id=product_id,
        tag_uid=tag_uid,
        event_type=event_type,
    )
    return [InventoryEventListItemRead.model_validate(event) for event in events]


@router.get("/tags/{tag_uid}/history", response_model=TagHistoryRead)
async def get_tag_history_endpoint(
    tag_uid: UUID,
    session: Annotated[AsyncSession, Depends(get_db_session)],
) -> TagHistoryRead:
    try:
        tag, usages = await get_tag_history(session, tag_uid)
    except (
        ProductNotFoundError,
        TagUsageNotFoundError,
        TagAlreadyInUseError,
        TagUsageClosedError,
        InvalidMovementQuantityError,
    ) as error:
        raise_inventory_http_error(error)

    return TagHistoryRead(
        tag_uid=tag.uid,
        usages=[TagUsageRead.model_validate(usage) for usage in usages],
    )


@router.post("/tags/{tag_uid}/shipments/partial", response_model=TagUsageRead)
async def create_partial_shipment(
    tag_uid: UUID,
    payload: PartialShipmentCreate,
    session: Annotated[AsyncSession, Depends(get_db_session)],
    current_user: Annotated[User, Depends(get_current_user)],
) -> TagUsageRead:
    try:
        usage = await ship_partial_by_tag_uid(
            session,
            tag_uid=tag_uid,
            quantity=payload.quantity,
            occurred_at=payload.occurred_at,
            note=payload.note,
            actor_user=current_user,
            source=InventoryEventSource.API,
        )
        await session.commit()
        reloaded_usage = await get_usage_by_id(session, usage.id)
    except (
        ProductNotFoundError,
        TagUsageNotFoundError,
        TagAlreadyInUseError,
        TagUsageClosedError,
        InvalidMovementQuantityError,
        TagProvisionError,
    ) as error:
        raise_inventory_http_error(error)

    return TagUsageRead.model_validate(reloaded_usage)


@router.post("/tags/{tag_uid}/shipments/full", response_model=TagUsageRead)
async def create_full_shipment(
    tag_uid: UUID,
    payload: FullShipmentCreate,
    session: Annotated[AsyncSession, Depends(get_db_session)],
    current_user: Annotated[User, Depends(get_current_user)],
) -> TagUsageRead:
    try:
        usage = await ship_full_by_tag_uid(
            session,
            tag_uid=tag_uid,
            occurred_at=payload.occurred_at,
            note=payload.note,
            actor_user=current_user,
            source=InventoryEventSource.API,
        )
        await session.commit()
        reloaded_usage = await get_usage_by_id(session, usage.id)
    except (
        ProductNotFoundError,
        TagUsageNotFoundError,
        TagAlreadyInUseError,
        TagUsageClosedError,
        InvalidMovementQuantityError,
        TagProvisionError,
    ) as error:
        raise_inventory_http_error(error)

    return TagUsageRead.model_validate(reloaded_usage)
