"""add badge uid and passwordless device auth support

Revision ID: 0003_badge_and_passwordless_device_auth
Revises: 0002_legacy_device_schema
Create Date: 2026-03-27 18:05:00.000000
"""

from __future__ import annotations

from alembic import op
import sqlalchemy as sa


revision = "0003_badge_auth"
down_revision = "0002_legacy_device_schema"
branch_labels = None
depends_on = None


def _sqlite_columns(table_name: str) -> set[str]:
    bind = op.get_bind()
    rows = bind.execute(sa.text(f"PRAGMA table_info({table_name})")).fetchall()
    return {row[1] for row in rows}


def upgrade() -> None:
    bind = op.get_bind()
    dialect = bind.dialect.name

    user_columns = _sqlite_columns("users") if dialect == "sqlite" else set()
    if dialect != "sqlite" or "badge_uid" not in user_columns:
        with op.batch_alter_table("users") as batch_op:
            batch_op.add_column(sa.Column("badge_uid", sa.String(length=64), nullable=True))
            batch_op.create_index("ix_users_badge_uid", ["badge_uid"], unique=True)

    if dialect == "sqlite":
        return

    op.alter_column("devices", "secret_hash", existing_type=sa.String(length=255), nullable=True)


def downgrade() -> None:
    bind = op.get_bind()
    dialect = bind.dialect.name

    if dialect != "sqlite":
        op.alter_column("devices", "secret_hash", existing_type=sa.String(length=255), nullable=False)

    with op.batch_alter_table("users") as batch_op:
        batch_op.drop_index("ix_users_badge_uid")
        batch_op.drop_column("badge_uid")
