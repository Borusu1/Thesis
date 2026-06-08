from __future__ import annotations

from pathlib import Path

from alembic import command
from alembic.config import Config
from sqlalchemy import create_engine, inspect


def test_alembic_upgrade_creates_expected_tables(tmp_path: Path) -> None:
    project_root = Path(__file__).resolve().parents[1]
    database_path = tmp_path / "alembic-test.db"
    database_url = f"sqlite+aiosqlite:///{database_path}"
    config = Config(str(project_root / "alembic.ini"))
    config.set_main_option("script_location", str(project_root / "alembic"))
    config.set_main_option("sqlalchemy.url", database_url)

    command.upgrade(config, "head")

    engine = create_engine(f"sqlite:///{database_path}")
    inspector = inspect(engine)

    assert set(inspector.get_table_names()) >= {
        "devices",
        "users",
        "products",
        "nfc_tags",
        "tag_usages",
        "inventory_events",
    }
