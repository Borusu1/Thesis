from sqlalchemy.orm import DeclarativeBase


class Base(DeclarativeBase):
    pass


# Import models so metadata is fully populated for tests and Alembic.
from app import models  # noqa: E402,F401
