export type ProductStatus = 'inStock' | 'outOfStock';

export type InventoryEventType = 'receipt' | 'shipment_partial' | 'shipment_full';

export type Product = {
  id: number;
  sku: number;
  name: string;
  description: string | null;
  quantityOnHand: number;
  createdAt: string;
  status: ProductStatus;
};

export type UsageEvent = {
  id: number;
  type: InventoryEventType;
  quantity: number;
  occurredAt: string;
  note: string | null;
};

export type TagUsage = {
  id: number;
  tagUid: string;
  productId: number;
  productNameSnapshot: string;
  quantityInitial: number;
  quantityCurrent: number;
  arrivedAt: string;
  warehouseLocation: string | null;
  closedAt: string | null;
  events: UsageEvent[];
};

export type ActiveTag = Omit<TagUsage, 'closedAt' | 'events'>;

export type TagHistory = {
  tagUid: string;
  usages: TagUsage[];
};

export type TagLookupResult = {
  tagUid: string;
  activeUsage: TagUsage | null;
  usages: TagUsage[];
};

export type OperationType = InventoryEventType;

export type Operation = {
  id: number;
  usageId: number;
  productId: number;
  productNameSnapshot: string;
  type: OperationType;
  quantity: number;
  quantityDelta: number;
  note: string | null;
  createdAt: string;
  actor: string;
  tagUid: string;
};

export type DashboardSummary = {
  totalProducts: number;
  totalUnits: number;
  lowStockCount: number;
  outOfStockCount: number;
  recentOperations: Operation[];
};
