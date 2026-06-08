"""upgrade legacy docker schema to device-capable schema"""

from collections.abc import Sequence

from alembic import op
import sqlalchemy as sa

revision = "0002_legacy_device_schema"
down_revision = "0001_baseline"
branch_labels = None
depends_on = None


def upgrade() -> None:
    bind = op.get_bind()
    dialect = bind.dialect.name
    inspector = sa.inspect(bind)

    def has_column(table_name: str, column_name: str) -> bool:
        return column_name in {column["name"] for column in inspector.get_columns(table_name)}

    user_role = sa.Enum("operator", "admin", name="user_role")
    inventory_event_source = sa.Enum("api", "device", "device_sync", name="inventory_event_source")
    user_role.create(bind, checkfirst=True)
    inventory_event_source.create(bind, checkfirst=True)

    if dialect == "sqlite":
        with op.batch_alter_table("users") as batch_op:
            if not has_column("users", "pin_hash"):
                batch_op.add_column(sa.Column("pin_hash", sa.String(length=255), nullable=True))
            if not has_column("users", "role"):
                batch_op.add_column(
                    sa.Column("role", sa.String(length=32), nullable=False, server_default="operator")
                )
    else:
        op.execute("ALTER TABLE users ADD COLUMN IF NOT EXISTS pin_hash VARCHAR(255)")
        op.execute("ALTER TABLE users ADD COLUMN IF NOT EXISTS role user_role DEFAULT 'operator' NOT NULL")

    if "devices" not in inspector.get_table_names():
        op.create_table(
            "devices",
            sa.Column("id", sa.Integer(), primary_key=True, autoincrement=True),
            sa.Column("device_id", sa.String(length=64), nullable=False),
            sa.Column("name", sa.String(length=255), nullable=False),
            sa.Column("secret_hash", sa.String(length=255), nullable=True if dialect == "sqlite" else False),
            sa.Column("is_active", sa.Boolean(), nullable=False, server_default=sa.true()),
            sa.Column("created_at", sa.DateTime(timezone=True), nullable=False, server_default=sa.func.now()),
            sa.Column("last_seen_at", sa.DateTime(timezone=True), nullable=True),
        )
    op.create_index("ix_devices_device_id", "devices", ["device_id"], unique=True, if_not_exists=True)

    if dialect == "sqlite":
        with op.batch_alter_table("products") as batch_op:
            if not has_column("products", "sku"):
                batch_op.add_column(sa.Column("sku", sa.Integer(), nullable=True))
    else:
        op.execute("ALTER TABLE products ADD COLUMN IF NOT EXISTS sku INTEGER")
    op.execute("UPDATE products SET sku = id WHERE sku IS NULL")
    if dialect != "sqlite":
        op.execute("ALTER TABLE products ALTER COLUMN sku SET NOT NULL")
    op.create_index("ix_products_sku", "products", ["sku"], unique=True, if_not_exists=True)

    if dialect == "sqlite":
        with op.batch_alter_table("nfc_tags") as batch_op:
            if not has_column("nfc_tags", "chip_uid_hex"):
                batch_op.add_column(sa.Column("chip_uid_hex", sa.String(length=64), nullable=True))
            if not has_column("nfc_tags", "provisioned_at"):
                batch_op.add_column(sa.Column("provisioned_at", sa.DateTime(timezone=True), nullable=True))
            if not has_column("nfc_tags", "provisioned_by_device_id"):
                batch_op.add_column(sa.Column("provisioned_by_device_id", sa.Integer(), nullable=True))
    else:
        op.execute("ALTER TABLE nfc_tags ADD COLUMN IF NOT EXISTS chip_uid_hex VARCHAR(64)")
        op.execute("ALTER TABLE nfc_tags ADD COLUMN IF NOT EXISTS provisioned_at TIMESTAMPTZ")
        op.execute("ALTER TABLE nfc_tags ADD COLUMN IF NOT EXISTS provisioned_by_device_id INTEGER")
        op.execute(
            """
            DO $$
            BEGIN
                IF NOT EXISTS (
                    SELECT 1
                    FROM information_schema.table_constraints
                    WHERE constraint_name = 'nfc_tags_provisioned_by_device_id_fkey'
                      AND table_name = 'nfc_tags'
                ) THEN
                    ALTER TABLE nfc_tags
                        ADD CONSTRAINT nfc_tags_provisioned_by_device_id_fkey
                        FOREIGN KEY (provisioned_by_device_id) REFERENCES devices(id);
                END IF;
            END
            $$;
            """
        )
    op.create_index("ix_nfc_tags_chip_uid_hex", "nfc_tags", ["chip_uid_hex"], unique=True, if_not_exists=True)

    if dialect == "sqlite":
        with op.batch_alter_table("inventory_events") as batch_op:
            if not has_column("inventory_events", "actor_user_id"):
                batch_op.add_column(sa.Column("actor_user_id", sa.Integer(), nullable=True))
            if not has_column("inventory_events", "actor_device_id"):
                batch_op.add_column(sa.Column("actor_device_id", sa.Integer(), nullable=True))
            if not has_column("inventory_events", "client_operation_id"):
                batch_op.add_column(sa.Column("client_operation_id", sa.String(length=64), nullable=True))
            if not has_column("inventory_events", "source"):
                batch_op.add_column(sa.Column("source", sa.String(length=32), nullable=False, server_default="api"))
    else:
        op.execute("ALTER TABLE inventory_events ADD COLUMN IF NOT EXISTS actor_user_id INTEGER")
        op.execute("ALTER TABLE inventory_events ADD COLUMN IF NOT EXISTS actor_device_id INTEGER")
        op.execute("ALTER TABLE inventory_events ADD COLUMN IF NOT EXISTS client_operation_id VARCHAR(64)")
        op.execute(
            "ALTER TABLE inventory_events ADD COLUMN IF NOT EXISTS source inventory_event_source DEFAULT 'api' NOT NULL"
        )
        op.execute(
            """
            DO $$
            BEGIN
                IF NOT EXISTS (
                    SELECT 1
                    FROM information_schema.table_constraints
                    WHERE constraint_name = 'inventory_events_actor_user_id_fkey'
                      AND table_name = 'inventory_events'
                ) THEN
                    ALTER TABLE inventory_events
                        ADD CONSTRAINT inventory_events_actor_user_id_fkey
                        FOREIGN KEY (actor_user_id) REFERENCES users(id);
                END IF;
            END
            $$;
            """
        )
        op.execute(
            """
            DO $$
            BEGIN
                IF NOT EXISTS (
                    SELECT 1
                    FROM information_schema.table_constraints
                    WHERE constraint_name = 'inventory_events_actor_device_id_fkey'
                      AND table_name = 'inventory_events'
                ) THEN
                    ALTER TABLE inventory_events
                        ADD CONSTRAINT inventory_events_actor_device_id_fkey
                        FOREIGN KEY (actor_device_id) REFERENCES devices(id);
                END IF;
            END
            $$;
            """
        )
    op.create_index(
        "ix_inventory_events_client_operation_id",
        "inventory_events",
        ["client_operation_id"],
        unique=True,
        if_not_exists=True,
    )


def downgrade() -> None:
    op.drop_index("ix_inventory_events_client_operation_id", table_name="inventory_events")
    op.drop_column("inventory_events", "source")
    op.drop_column("inventory_events", "client_operation_id")
    op.drop_column("inventory_events", "actor_device_id")
    op.drop_column("inventory_events", "actor_user_id")

    op.drop_index("ix_nfc_tags_chip_uid_hex", table_name="nfc_tags")
    op.drop_column("nfc_tags", "provisioned_by_device_id")
    op.drop_column("nfc_tags", "provisioned_at")
    op.drop_column("nfc_tags", "chip_uid_hex")

    op.drop_index("ix_products_sku", table_name="products")
    op.drop_column("products", "sku")

    op.drop_index("ix_devices_device_id", table_name="devices")
    op.drop_table("devices")

    op.drop_column("users", "role")
    op.drop_column("users", "pin_hash")

    bind = op.get_bind()
    sa.Enum(name="inventory_event_source").drop(bind, checkfirst=True)
    sa.Enum(name="user_role").drop(bind, checkfirst=True)
