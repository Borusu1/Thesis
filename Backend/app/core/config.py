import json
from functools import lru_cache
from typing import Annotated

from pydantic import field_validator

from pydantic_settings import BaseSettings, NoDecode, SettingsConfigDict


class Settings(BaseSettings):
    model_config = SettingsConfigDict(
        env_file=".env",
        env_file_encoding="utf-8",
        case_sensitive=False,
    )

    app_name: str = "Warehouse NFC Backend"
    app_env: str = "local"
    debug: bool = False
    host: str = "0.0.0.0"
    port: int = 8000
    database_url: str | None = None

    postgres_user: str = "warehouse"
    postgres_password: str = "warehouse"
    postgres_host: str = "localhost"
    postgres_port: int = 5432
    postgres_db: str = "warehouse"
    jwt_secret_key: str = "change-me"
    jwt_algorithm: str = "HS256"
    access_token_expire_minutes: int = 30
    backend_cors_origins: Annotated[list[str], NoDecode] = [
        "http://localhost:19006",
        "http://127.0.0.1:19006",
    ]
    seed_demo_user: bool | None = None
    demo_user_email: str = "demo@example.com"
    demo_user_password: str = "demo123"

    @field_validator("backend_cors_origins", mode="before")
    @classmethod
    def parse_backend_cors_origins(cls, value: object) -> object:
        if isinstance(value, str):
            normalized_value = value.strip()
            if not normalized_value:
                return []
            if normalized_value.startswith("["):
                return json.loads(normalized_value)
            return [item.strip() for item in normalized_value.split(",") if item.strip()]
        return value

    @property
    def sqlalchemy_database_url(self) -> str:
        if self.database_url:
            return self.database_url

        return (
            f"postgresql+asyncpg://{self.postgres_user}:{self.postgres_password}"
            f"@{self.postgres_host}:{self.postgres_port}/{self.postgres_db}"
        )

    @property
    def backend_cors_origin_regex(self) -> str | None:
        if self.app_env in {"local", "docker"}:
            return (
                r"^https?://(localhost|127\.0\.0\.1"
                r"|192\.168\.\d{1,3}\.\d{1,3}"
                r"|10\.\d{1,3}\.\d{1,3}\.\d{1,3}"
                r")(:\d+)?$"
            )
        return None

    @property
    def seed_demo_user_enabled(self) -> bool:
        if self.seed_demo_user is not None:
            return self.seed_demo_user

        return self.app_env in {"local", "docker"}


@lru_cache
def get_settings() -> Settings:
    return Settings()
