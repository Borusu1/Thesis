"""baseline schema"""

from collections.abc import Sequence

from alembic import op
import sqlalchemy as sa

revision = "0001_baseline"
down_revision = None
branch_labels = None
depends_on = None


inventory_event_type = sa.Enum(
    "receipt",
    "shipment_partial",
    "shipment_full",
    name="inventory_event_type",
)

inventory_event_source = sa.Enum(
    "api",
    "device",
    "device_sync",
    name="inventory_event_source",
)

user_role = sa.Enum(
    "operator",
    "admin",
    name="user_role",
)


def upgrade() -> None:
    op.create_table(
        "users",
        sa.Column("id", sa.Integer(), autoincrement=True, nullable=False),
        sa.Column("email", sa.String(length=255), nullable=False),
        sa.Column("password_hash", sa.String(length=255), nullable=False),
        sa.Column("pin_hash", sa.String(length=255), nullable=True),
        sa.Column("role", user_role, server_default="operator", nullable=False),
        sa.Column("is_active", sa.Boolean(), nullable=False),
        sa.Column("created_at", sa.DateTime(timezone=True), server_default=sa.func.now(), nullable=False),
        sa.PrimaryKeyConstraint("id"),
        sa.UniqueConstraint("email"),
    )
    op.create_index(op.f("ix_users_email"), "users", ["email"], unique=True)

    op.create_table(
        "devices",
        sa.Column("id", sa.Integer(), autoincrement=True, nullable=False),
        sa.Column("device_id", sa.String(length=64), nullable=False),
        sa.Column("name", sa.String(length=255), nullable=False),
        sa.Column("secret_hash", sa.String(length=255), nullable=False),
        sa.Column("is_active", sa.Boolean(), nullable=False),
        sa.Column("created_at", sa.DateTime(timezone=True), server_default=sa.func.now(), nullable=False),
        sa.Column("last_seen_at", sa.DateTime(timezone=True), nullable=True),
        sa.PrimaryKeyConstraint("id"),
        sa.UniqueConstraint("device_id"),
    )
    op.create_index(op.f("ix_devices_device_id"), "devices", ["device_id"], unique=True)

    op.create_table(
        "products",
        sa.Column("id", sa.Integer(), autoincrement=True, nullable=False),
        sa.Column("sku", sa.Integer(), nullable=False),
        sa.Column("name", sa.String(length=255), nullable=False),
        sa.Column("description", sa.Text(), nullable=True),
        sa.Column("quantity_on_hand", sa.Integer(), server_default="0", nullable=False),
        sa.Column("created_at", sa.DateTime(timezone=True), server_default=sa.func.now(), nullable=False),
        sa.CheckConstraint("quantity_on_hand >= 0", name="ck_products_quantity_on_hand_non_negative"),
        sa.PrimaryKeyConstraint("id"),
        sa.UniqueConstraint("sku"),
    )
    op.create_index(op.f("ix_products_sku"), "products", ["sku"], unique=True)

    op.create_table(
        "nfc_tags",
        sa.Column("id", sa.Integer(), autoincrement=True, nullable=False),
        sa.Column("uid", sa.Uuid(), nullable=False),
        sa.Column("chip_uid_hex", sa.String(length=64), nullable=True),
        sa.Column("provisioned_at", sa.DateTime(timezone=True), nullable=True),
        sa.Column("provisioned_by_device_id", sa.Integer(), nullable=True),
        sa.Column("created_at", sa.DateTime(timezone=True), server_default=sa.func.now(), nullable=False),
        sa.ForeignKeyConstraint(["provisioned_by_device_id"], ["devices.id"]),
        sa.PrimaryKeyConstraint("id"),
        sa.UniqueConstraint("chip_uid_hex"),
        sa.UniqueConstraint("uid"),
    )
    op.create_index(op.f("ix_nfc_tags_uid"), "nfc_tags", ["uid"], unique=True)

    op.create_table(
        "tag_usages",
        sa.Column("id", sa.Integer(), autoincrement=True, nullable=False),
        sa.Column("tag_id", sa.Integer(), nullable=False),
        sa.Column("product_id", sa.Integer(), nullable=False),
        sa.Column("product_name_snapshot", sa.String(length=255), nullable=False),
        sa.Column("quantity_initial", sa.Integer(), nullable=False),
        sa.Column("quantity_current", sa.Integer(), nullable=False),
        sa.Column("arrived_at", sa.DateTime(timezone=True), nullable=False),
        sa.Column("warehouse_location", sa.String(length=255), nullable=True),
        sa.Column("closed_at", sa.DateTime(timezone=True), nullable=True),
        sa.CheckConstraint("quantity_current <= quantity_initial", name="ck_tag_usages_quantity_current_lte_initial"),
        sa.CheckConstraint("quantity_current >= 0", name="ck_tag_usages_quantity_current_non_negative"),
        sa.CheckConstraint("quantity_initial > 0", name="ck_tag_usages_quantity_initial_positive"),
        sa.ForeignKeyConstraint(["product_id"], ["products.id"]),
        sa.ForeignKeyConstraint(["tag_id"], ["nfc_tags.id"]),
        sa.PrimaryKeyConstraint("id"),
    )
    op.create_index(
        "uq_tag_usages_active_tag",
        "tag_usages",
        ["tag_id"],
        unique=True,
        postgresql_where=sa.text("closed_at IS NULL"),
        sqlite_where=sa.text("closed_at IS NULL"),
    )

    op.create_table(
        "inventory_events",
        sa.Column("id", sa.Integer(), autoincrement=True, nullable=False),
        sa.Column("usage_id", sa.Integer(), nullable=False),
        sa.Column("event_type", inventory_event_type, nullable=False),
        sa.Column("quantity", sa.Integer(), nullable=False),
        sa.Column("occurred_at", sa.DateTime(timezone=True), nullable=False),
        sa.Column("note", sa.Text(), nullable=True),
        sa.Column("actor_user_id", sa.Integer(), nullable=True),
        sa.Column("actor_device_id", sa.Integer(), nullable=True),
        sa.Column("client_operation_id", sa.String(length=64), nullable=True),
        sa.Column("source", inventory_event_source, server_default="api", nullable=False),
        sa.CheckConstraint("quantity > 0", name="ck_inventory_events_quantity_positive"),
        sa.ForeignKeyConstraint(["actor_device_id"], ["devices.id"]),
        sa.ForeignKeyConstraint(["actor_user_id"], ["users.id"]),
        sa.ForeignKeyConstraint(["usage_id"], ["tag_usages.id"]),
        sa.PrimaryKeyConstraint("id"),
        sa.UniqueConstraint("client_operation_id"),
    )


def downgrade() -> None:
    bind = op.get_bind()

    op.drop_table("inventory_events")
    op.drop_index("uq_tag_usages_active_tag", table_name="tag_usages")
    op.drop_table("tag_usages")
    op.drop_index(op.f("ix_nfc_tags_uid"), table_name="nfc_tags")
    op.drop_table("nfc_tags")
    op.drop_index(op.f("ix_products_sku"), table_name="products")
    op.drop_table("products")
    op.drop_index(op.f("ix_devices_device_id"), table_name="devices")
    op.drop_table("devices")
    op.drop_index(op.f("ix_users_email"), table_name="users")
    op.drop_table("users")

    if bind.dialect.name == "postgresql":
        inventory_event_type.drop(bind, checkfirst=True)
        inventory_event_source.drop(bind, checkfirst=True)
        user_role.drop(bind, checkfirst=True)
