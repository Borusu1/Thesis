from typing import Annotated
from uuid import UUID

from fastapi import APIRouter, Depends, HTTPException, Query, status
from sqlalchemy.ext.asyncio import AsyncSession

from app.api.dependencies.auth import get_current_device
from app.db.session import get_db_session
from app.models import Device, InventoryEventSource
from app.schemas import (
    DeviceLoginRequest,
    DeviceLoginResponse,
    DeviceSyncRequest,
    DeviceSyncResponse,
    DeviceSyncResult,
    DeviceTagLookupRead,
    ProductRead,
    ProductRecentRead,
    TagProvisionRequest,
    TagProvisionResponse,
    TagUsageRead,
)
from app.services import (
    InvalidMovementQuantityError,
    ProductNotFoundError,
    TagAlreadyInUseError,
    TagProvisionError,
    TagUsageClosedError,
    TagUsageNotFoundError,
    authenticate_device,
    get_product_by_sku,
    list_recent_products,
    lookup_tag,
    provision_tag,
    receive_product,
    reissue_tag,
    ship_full_by_tag_uid,
    ship_partial_by_tag_uid,
)
from app.core.security import create_access_token

router = APIRouter(prefix="/device", tags=["device"])


def raise_device_http_error(error: Exception) -> None:
    if isinstance(error, (ProductNotFoundError, TagUsageNotFoundError)):
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail=str(error)) from error

    if isinstance(error, (TagAlreadyInUseError, TagUsageClosedError, InvalidMovementQuantityError, TagProvisionError)):
        raise HTTPException(status_code=status.HTTP_409_CONFLICT, detail=str(error)) from error

    raise error


@router.post("/auth/login", response_model=DeviceLoginResponse)
async def login_device(
    payload: DeviceLoginRequest,
    session: Annotated[AsyncSession, Depends(get_db_session)],
) -> DeviceLoginResponse:
    device = await authenticate_device(session, payload.device_id)
    if device is None:
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="Incorrect device credentials")

    await session.commit()
    access_token = create_access_token(subject=str(device.id), token_type="device")
    return DeviceLoginResponse(
        access_token=access_token,
        device_id=device.device_id,
        device_name=device.name,
    )


@router.get("/products/recent", response_model=ProductRecentRead)
async def get_recent_products(
    _: Annotated[Device, Depends(get_current_device)],
    session: Annotated[AsyncSession, Depends(get_db_session)],
    limit: int = Query(default=6, ge=1, le=12),
) -> ProductRecentRead:
    products = await list_recent_products(session, limit=limit)
    return ProductRecentRead(products=[ProductRead.model_validate(product) for product in products])


@router.get("/products/search", response_model=ProductRead)
async def search_product_by_sku(
    _: Annotated[Device, Depends(get_current_device)],
    session: Annotated[AsyncSession, Depends(get_db_session)],
    sku: int = Query(ge=1),
) -> ProductRead:
    product = await get_product_by_sku(session, sku)
    if product is None:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail=f"Product with SKU {sku} was not found.")
    return ProductRead.model_validate(product)


@router.get("/tags/{tag_uid}/lookup", response_model=DeviceTagLookupRead)
async def lookup_tag_endpoint(
    tag_uid: UUID,
    _: Annotated[Device, Depends(get_current_device)],
    session: Annotated[AsyncSession, Depends(get_db_session)],
) -> DeviceTagLookupRead:
    try:
        tag, active_usage, usages = await lookup_tag(session, tag_uid)
    except (ProductNotFoundError, TagUsageNotFoundError, TagAlreadyInUseError, TagUsageClosedError, InvalidMovementQuantityError, TagProvisionError) as error:
        raise_device_http_error(error)

    return DeviceTagLookupRead(
        tag_uid=tag.uid,
        chip_uid_hex=tag.chip_uid_hex,
        provisioned_at=tag.provisioned_at,
        active_usage=TagUsageRead.model_validate(active_usage) if active_usage else None,
        usages=[TagUsageRead.model_validate(usage) for usage in usages],
    )


@router.post("/tags/provision", response_model=TagProvisionResponse)
async def provision_tag_endpoint(
    payload: TagProvisionRequest,
    device: Annotated[Device, Depends(get_current_device)],
    session: Annotated[AsyncSession, Depends(get_db_session)],
) -> TagProvisionResponse:
    try:
        tag, reissued = await provision_tag(
            session,
            tag_uid=payload.tag_uid,
            chip_uid_hex=payload.chip_uid_hex,
            actor_device=device,
        )
        await session.commit()
    except (ProductNotFoundError, TagUsageNotFoundError, TagAlreadyInUseError, TagUsageClosedError, InvalidMovementQuantityError, TagProvisionError) as error:
        raise_device_http_error(error)

    return TagProvisionResponse(
        tag_uid=tag.uid,
        chip_uid_hex=tag.chip_uid_hex,
        provisioned_at=tag.provisioned_at,
        provisioned_by_device_id=tag.provisioned_by_device_id,
        reissued=reissued,
    )


@router.post("/tags/reissue", response_model=TagProvisionResponse)
async def reissue_tag_endpoint(
    payload: TagProvisionRequest,
    device: Annotated[Device, Depends(get_current_device)],
    session: Annotated[AsyncSession, Depends(get_db_session)],
) -> TagProvisionResponse:
    if not payload.chip_uid_hex:
        raise HTTPException(status_code=status.HTTP_422_UNPROCESSABLE_ENTITY, detail="chip_uid_hex is required.")

    try:
        tag = await reissue_tag(
            session,
            tag_uid=payload.tag_uid,
            new_chip_uid_hex=payload.chip_uid_hex,
            actor_device=device,
        )
        await session.commit()
    except (ProductNotFoundError, TagUsageNotFoundError, TagAlreadyInUseError, TagUsageClosedError, InvalidMovementQuantityError, TagProvisionError) as error:
        raise_device_http_error(error)

    return TagProvisionResponse(
        tag_uid=tag.uid,
        chip_uid_hex=tag.chip_uid_hex,
        provisioned_at=tag.provisioned_at,
        provisioned_by_device_id=tag.provisioned_by_device_id,
        reissued=True,
    )


@router.post("/operations/sync", response_model=DeviceSyncResponse)
async def sync_operations(
    payload: DeviceSyncRequest,
    device: Annotated[Device, Depends(get_current_device)],
    session: Annotated[AsyncSession, Depends(get_db_session)],
) -> DeviceSyncResponse:
    results: list[DeviceSyncResult] = []
    try:
        for operation in payload.operations:
            if operation.operation_type == "receipt":
                usage = await receive_product(
                    session,
                    product_id=operation.payload.product_id,
                    tag_uid=operation.payload.tag_uid,
                    quantity=operation.payload.quantity,
                    note=operation.payload.note,
                    actor_user=None,
                    actor_device=device,
                    client_operation_id=operation.client_operation_id,
                    source=InventoryEventSource.DEVICE_SYNC,
                    chip_uid_hex=operation.payload.chip_uid_hex,
                )
            elif operation.operation_type == "shipment_partial":
                usage = await ship_partial_by_tag_uid(
                    session,
                    tag_uid=operation.payload.tag_uid,
                    quantity=operation.payload.quantity,
                    note=operation.payload.note,
                    actor_user=None,
                    actor_device=device,
                    client_operation_id=operation.client_operation_id,
                    source=InventoryEventSource.DEVICE_SYNC,
                )
            else:
                usage = await ship_full_by_tag_uid(
                    session,
                    tag_uid=operation.payload.tag_uid,
                    note=operation.payload.note,
                    actor_user=None,
                    actor_device=device,
                    client_operation_id=operation.client_operation_id,
                    source=InventoryEventSource.DEVICE_SYNC,
                )

            results.append(
                DeviceSyncResult(
                    client_operation_id=operation.client_operation_id,
                    operation_type=operation.operation_type,
                    status="applied",
                    usage=TagUsageRead.model_validate(usage),
                )
            )

        await session.commit()
    except (ProductNotFoundError, TagUsageNotFoundError, TagAlreadyInUseError, TagUsageClosedError, InvalidMovementQuantityError, TagProvisionError) as error:
        await session.rollback()
        raise_device_http_error(error)

    return DeviceSyncResponse(results=results)
