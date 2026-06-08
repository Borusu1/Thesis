from contextlib import asynccontextmanager

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

from app.api.router import api_router
from app.core.config import get_settings
from app.db.session import AsyncSessionFactory, engine
from app.services.users import seed_demo_user


@asynccontextmanager
async def lifespan(_: FastAPI):
    settings = get_settings()

    if settings.seed_demo_user_enabled:
        async with AsyncSessionFactory() as session:
            await seed_demo_user(
                session,
                email=settings.demo_user_email,
                password=settings.demo_user_password,
            )
            await session.commit()

    yield
    await engine.dispose()


def create_app() -> FastAPI:
    settings = get_settings()

    application = FastAPI(
        title=settings.app_name,
        debug=settings.debug,
        lifespan=lifespan,
    )
    if settings.backend_cors_origins:
        application.add_middleware(
            CORSMiddleware,
            allow_origins=settings.backend_cors_origins,
            allow_origin_regex=settings.backend_cors_origin_regex,
            allow_credentials=True,
            allow_methods=["*"],
            allow_headers=["*"],
        )
    application.include_router(api_router)
    return application


app = create_app()
