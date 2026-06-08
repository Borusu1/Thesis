import { AppSettings, UserSession } from '@/src/types/app';
import {
  ActiveTag,
  DashboardSummary,
  InventoryEventType,
  Operation,
  Product,
  TagHistory,
  TagLookupResult,
  TagUsage,
} from '@/src/types/warehouse';

export type CreateProductInput = {
  sku: number;
  name: string;
  description?: string;
};

export type InventoryEventFilter = {
  productId?: number;
  tagUid?: string;
  eventType?: InventoryEventType;
};

export type CreatePartialShipmentInput = {
  tagUid: string;
  quantity: number;
  note?: string;
};

export type CreateFullShipmentInput = {
  tagUid: string;
  note?: string;
};

export interface WarehouseDataService {
  login(username: string, password: string): Promise<UserSession>;
  logout(): Promise<void>;
  getSession(): Promise<UserSession | null>;
  getSettings(): Promise<AppSettings>;
  updateSettings(input: Partial<AppSettings>): Promise<AppSettings>;
  getDashboardSummary(): Promise<DashboardSummary>;
  getProducts(): Promise<Product[]>;
  getProductById(productId: string | number): Promise<Product | null>;
  createProduct(input: CreateProductInput): Promise<Product>;
  getOperations(filters?: InventoryEventFilter): Promise<Operation[]>;
  createPartialShipment(input: CreatePartialShipmentInput): Promise<TagUsage>;
  createFullShipment(input: CreateFullShipmentInput): Promise<TagUsage>;
  getActiveTags(productId?: number): Promise<ActiveTag[]>;
  getTagHistory(tagUid: string): Promise<TagHistory | null>;
  lookupTag(tagUid: string): Promise<TagLookupResult | null>;
}
