from collections.abc import AsyncGenerator

from sqlalchemy.ext.asyncio import AsyncSession, async_sessionmaker, create_async_engine

from app.core.config import get_settings

settings = get_settings()
database_url = settings.sqlalchemy_database_url

connect_args: dict[str, bool] = {}
if database_url.startswith("sqlite+aiosqlite"):
    connect_args["check_same_thread"] = False

engine = create_async_engine(
    database_url,
    echo=settings.debug,
    connect_args=connect_args,
)

AsyncSessionFactory = async_sessionmaker(
    bind=engine,
    expire_on_commit=False,
    class_=AsyncSession,
)


async def get_db_session() -> AsyncGenerator[AsyncSession, None]:
    async with AsyncSessionFactory() as session:
        yield session
