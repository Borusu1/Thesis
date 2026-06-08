from typing import Annotated

from fastapi import APIRouter, Depends, HTTPException, status
from sqlalchemy.ext.asyncio import AsyncSession

from app.api.dependencies.auth import get_current_user
from app.db.session import get_db_session
from app.schemas import ProductCreate, ProductRead
from app.services import ProductNotFoundError, create_product, get_product, list_products

router = APIRouter(
    prefix="/products",
    tags=["products"],
    dependencies=[Depends(get_current_user)],
)


@router.post("", response_model=ProductRead, status_code=status.HTTP_201_CREATED)
async def create_product_endpoint(
    payload: ProductCreate,
    session: Annotated[AsyncSession, Depends(get_db_session)],
) -> ProductRead:
    product = await create_product(
        session,
        sku=payload.sku,
        name=payload.name,
        description=payload.description,
    )
    await session.commit()
    return ProductRead.model_validate(product)


@router.get("", response_model=list[ProductRead])
async def list_products_endpoint(
    session: Annotated[AsyncSession, Depends(get_db_session)],
) -> list[ProductRead]:
    products = await list_products(session)
    return [ProductRead.model_validate(product) for product in products]


@router.get("/{product_id}", response_model=ProductRead)
async def get_product_endpoint(
    product_id: int,
    session: Annotated[AsyncSession, Depends(get_db_session)],
) -> ProductRead:
    try:
        product = await get_product(session, product_id)
    except ProductNotFoundError as error:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail=str(error)) from error

    return ProductRead.model_validate(product)
