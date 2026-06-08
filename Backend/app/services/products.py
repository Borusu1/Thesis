from sqlalchemy import desc, select
from sqlalchemy.ext.asyncio import AsyncSession

from app.models import Product
from app.services.inventory import ProductNotFoundError


async def create_product(
    session: AsyncSession,
    *,
    sku: int,
    name: str,
    description: str | None = None,
) -> Product:
    product = Product(sku=sku, name=name, description=description)
    session.add(product)
    await session.flush()
    return product


async def list_products(session: AsyncSession) -> list[Product]:
    result = await session.execute(
        select(Product).order_by(desc(Product.created_at), desc(Product.id))
    )
    return list(result.scalars())


async def get_product(session: AsyncSession, product_id: int) -> Product:
    product = await session.get(Product, product_id)
    if product is None:
        raise ProductNotFoundError(f"Product {product_id} was not found.")
    return product


async def get_product_by_sku(session: AsyncSession, sku: int) -> Product | None:
    result = await session.execute(select(Product).where(Product.sku == sku))
    return result.scalar_one_or_none()


async def list_recent_products(session: AsyncSession, *, limit: int = 6) -> list[Product]:
    result = await session.execute(
        select(Product).order_by(desc(Product.created_at), desc(Product.id)).limit(limit)
    )
    return list(result.scalars())
