import AsyncStorage from '@react-native-async-storage/async-storage';
import { Platform } from 'react-native';
import { getLocales } from 'expo-localization';

import {
  CreateFullShipmentInput,
  CreatePartialShipmentInput,
  CreateProductInput,
  InventoryEventFilter,
  WarehouseDataService,
} from '@/src/services/warehouse/types';
import { AppSettings, LocaleCode, UserSession } from '@/src/types/app';
import {
  ActiveTag,
  DashboardSummary,
  Operation,
  Product,
  TagHistory,
  TagLookupResult,
  TagUsage,
  UsageEvent,
} from '@/src/types/warehouse';
import { normalizeTagUid } from '@/src/utils/tag';
import { resolveProductStatus } from '@/src/utils/warehouse';

const SESSION_STORAGE_KEY = 'warehouse.session';
const SETTINGS_STORAGE_KEY = 'warehouse.settings';

type StoredSession = {
  accessToken: string;
  user: BackendUserRead;
};

type BackendTokenResponse = {
  access_token: string;
  token_type: string;
};

type BackendUserRead = {
  id: number;
  email: string;
  role: string;
  is_active: boolean;
  created_at: string;
};

type BackendProductRead = {
  id: number;
  sku: number;
  name: string;
  description: string | null;
  quantity_on_hand: number;
  created_at: string;
};

type BackendInventoryEventRead = {
  id: number;
  event_type: UsageEvent['type'];
  quantity: number;
  occurred_at: string;
  note: string | null;
};

type BackendInventoryEventListItemRead = {
  id: number;
  usage_id: number;
  product_id: number;
  product_name_snapshot: string;
  tag_uid: string;
  event_type: Operation['type'];
  quantity: number;
  occurred_at: string;
  note: string | null;
  source: 'api' | 'device_sync';
};

type BackendTagUsageRead = {
  id: number;
  tag_uid: string;
  product_id: number;
  product_name_snapshot: string;
  quantity_initial: number;
  quantity_current: number;
  arrived_at: string;
  warehouse_location: string | null;
  closed_at: string | null;
  events: BackendInventoryEventRead[];
};

type BackendActiveTagRead = {
  id: number;
  tag_uid: string;
  product_id: number;
  product_name_snapshot: string;
  quantity_initial: number;
  quantity_current: number;
  arrived_at: string;
  warehouse_location: string | null;
};

type BackendTagHistoryRead = {
  tag_uid: string;
  usages: BackendTagUsageRead[];
};

class ApiError extends Error {
  status: number;

  constructor(message: string, status: number) {
    super(message);
    this.name = 'ApiError';
    this.status = status;
  }
}

function getDefaultApiBaseUrl() {
  if (Platform.OS === 'android') {
    return 'http://10.0.2.2:8000';
  }

  return 'http://127.0.0.1:8000';
}

function getApiBaseUrl() {
  return (process.env.EXPO_PUBLIC_API_BASE_URL ?? getDefaultApiBaseUrl()).replace(/\/+$/, '');
}

function getDefaultLanguage(): LocaleCode {
  return getLocales()[0]?.languageCode === 'en' ? 'en' : 'uk';
}

function toUserSession(storedSession: StoredSession): UserSession {
  return {
    id: storedSession.user.id,
    email: storedSession.user.email,
    isActive: storedSession.user.is_active,
    accessToken: storedSession.accessToken,
  };
}

function toUsageEvent(event: BackendInventoryEventRead): UsageEvent {
  return {
    id: event.id,
    type: event.event_type,
    quantity: event.quantity,
    occurredAt: event.occurred_at,
    note: event.note,
  };
}

function toProduct(product: BackendProductRead): Product {
  return {
    id: product.id,
    sku: product.sku,
    name: product.name,
    description: product.description,
    quantityOnHand: product.quantity_on_hand,
    createdAt: product.created_at,
    status: resolveProductStatus(product.quantity_on_hand),
  };
}

function toTagUsage(usage: BackendTagUsageRead): TagUsage {
  return {
    id: usage.id,
    tagUid: normalizeTagUid(usage.tag_uid),
    productId: usage.product_id,
    productNameSnapshot: usage.product_name_snapshot,
    quantityInitial: usage.quantity_initial,
    quantityCurrent: usage.quantity_current,
    arrivedAt: usage.arrived_at,
    warehouseLocation: usage.warehouse_location,
    closedAt: usage.closed_at,
    events: usage.events.map(toUsageEvent),
  };
}

function toActiveTag(tag: BackendActiveTagRead): ActiveTag {
  return {
    id: tag.id,
    tagUid: normalizeTagUid(tag.tag_uid),
    productId: tag.product_id,
    productNameSnapshot: tag.product_name_snapshot,
    quantityInitial: tag.quantity_initial,
    quantityCurrent: tag.quantity_current,
    arrivedAt: tag.arrived_at,
    warehouseLocation: tag.warehouse_location,
  };
}

function toOperation(event: BackendInventoryEventListItemRead): Operation {
  const normalizedTagUid = normalizeTagUid(event.tag_uid);
  return {
    id: event.id,
    usageId: event.usage_id,
    productId: event.product_id,
    productNameSnapshot: event.product_name_snapshot,
    type: event.event_type,
    quantity: event.quantity,
    quantityDelta: event.event_type === 'receipt' ? event.quantity : -event.quantity,
    note: event.note,
    createdAt: event.occurred_at,
    actor: event.source === 'device_sync' ? 'Device' : 'API',
    tagUid: normalizedTagUid,
  };
}

function buildDashboardSummary(products: Product[], operations: Operation[]): DashboardSummary {
  return {
    totalProducts: products.length,
    totalUnits: products.reduce((sum, product) => sum + product.quantityOnHand, 0),
    lowStockCount: 0,
    outOfStockCount: products.filter((product) => product.status === 'outOfStock').length,
    recentOperations: operations.slice(0, 5),
  };
}

async function readStoredSession() {
  const rawSession = await AsyncStorage.getItem(SESSION_STORAGE_KEY);

  return rawSession ? (JSON.parse(rawSession) as StoredSession) : null;
}

async function persistStoredSession(session: StoredSession) {
  await AsyncStorage.setItem(SESSION_STORAGE_KEY, JSON.stringify(session));
}

async function clearStoredSession() {
  await AsyncStorage.removeItem(SESSION_STORAGE_KEY);
}

export class ApiWarehouseDataService implements WarehouseDataService {
  private async request<T>(
    path: string,
    init: RequestInit = {},
    accessToken?: string | null
  ): Promise<T> {
    const headers = new Headers(init.headers);

    if (accessToken) {
      headers.set('Authorization', `Bearer ${accessToken}`);
    }

    const response = await fetch(`${getApiBaseUrl()}${path}`, {
      ...init,
      headers,
    });

    if (response.status === 401 && accessToken) {
      await clearStoredSession();
    }

    if (!response.ok) {
      let detail = `${response.status}`;

      try {
        const payload = (await response.json()) as { detail?: string };
        detail = payload.detail ?? detail;
      } catch {
        // Ignore non-JSON error payloads.
      }

      throw new ApiError(detail, response.status);
    }

    return (await response.json()) as T;
  }

  private async getAuthorized<T>(path: string) {
    const storedSession = await readStoredSession();

    if (!storedSession) {
      throw new ApiError('UNAUTHORIZED', 401);
    }

    return this.request<T>(path, {}, storedSession.accessToken);
  }

  async login(username: string, password: string): Promise<UserSession> {
    const payload = new URLSearchParams();
    payload.set('username', username.trim());
    payload.set('password', password);

    const token = await this.request<BackendTokenResponse>('/api/v1/auth/login', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/x-www-form-urlencoded',
      },
      body: payload.toString(),
    });

    const user = await this.request<BackendUserRead>('/api/v1/auth/me', {}, token.access_token);
    const storedSession = {
      accessToken: token.access_token,
      user,
    };

    await persistStoredSession(storedSession);

    return toUserSession(storedSession);
  }

  async logout(): Promise<void> {
    await clearStoredSession();
  }

  async getSession(): Promise<UserSession | null> {
    const storedSession = await readStoredSession();

    if (!storedSession) {
      return null;
    }

    try {
      const refreshedUser = await this.request<BackendUserRead>(
        '/api/v1/auth/me',
        {},
        storedSession.accessToken
      );
      const nextStoredSession = {
        accessToken: storedSession.accessToken,
        user: refreshedUser,
      };

      await persistStoredSession(nextStoredSession);

      return toUserSession(nextStoredSession);
    } catch (error) {
      if (error instanceof ApiError && error.status === 401) {
        return null;
      }

      throw error;
    }
  }

  async getSettings(): Promise<AppSettings> {
    const rawSettings = await AsyncStorage.getItem(SETTINGS_STORAGE_KEY);

    if (!rawSettings) {
      return { language: getDefaultLanguage() };
    }

    return JSON.parse(rawSettings) as AppSettings;
  }

  async updateSettings(input: Partial<AppSettings>): Promise<AppSettings> {
    const currentSettings = await this.getSettings();
    const nextSettings = {
      ...currentSettings,
      ...input,
    };

    await AsyncStorage.setItem(SETTINGS_STORAGE_KEY, JSON.stringify(nextSettings));

    return nextSettings;
  }

  async getDashboardSummary(): Promise<DashboardSummary> {
    const [products, operations] = await Promise.all([this.getProducts(), this.getOperations()]);

    return buildDashboardSummary(products, operations);
  }

  async getProducts(): Promise<Product[]> {
    const products = await this.getAuthorized<BackendProductRead[]>('/api/v1/products');

    return products.map(toProduct);
  }

  async getProductById(productId: string | number): Promise<Product | null> {
    try {
      const product = await this.getAuthorized<BackendProductRead>(`/api/v1/products/${productId}`);
      return toProduct(product);
    } catch (error) {
      if (error instanceof ApiError && error.status === 404) {
        return null;
      }

      throw error;
    }
  }

  async createProduct(input: CreateProductInput): Promise<Product> {
    const storedSession = await readStoredSession();
    const product = await this.request<BackendProductRead>(
      '/api/v1/products',
      {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({
          sku: input.sku,
          name: input.name.trim(),
          description: input.description?.trim() || null,
        }),
      },
      storedSession?.accessToken
    );

    return toProduct(product);
  }

  async getOperations(filters: InventoryEventFilter = {}): Promise<Operation[]> {
    const query = new URLSearchParams();

    if (filters.productId !== undefined) {
      query.set('product_id', String(filters.productId));
    }

    if (filters.tagUid) {
      query.set('tag_uid', normalizeTagUid(filters.tagUid));
    }

    if (filters.eventType) {
      query.set('event_type', filters.eventType);
    }

    const suffix = query.size ? `?${query.toString()}` : '';
    const events = await this.getAuthorized<BackendInventoryEventListItemRead[]>(
      `/api/v1/inventory/events${suffix}`
    );

    return events.map(toOperation);
  }

  async createPartialShipment(input: CreatePartialShipmentInput): Promise<TagUsage> {
    const storedSession = await readStoredSession();
    const usage = await this.request<BackendTagUsageRead>(
      `/api/v1/inventory/tags/${normalizeTagUid(input.tagUid)}/shipments/partial`,
      {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({
          quantity: input.quantity,
          note: input.note?.trim() || null,
        }),
      },
      storedSession?.accessToken
    );

    return toTagUsage(usage);
  }

  async createFullShipment(input: CreateFullShipmentInput): Promise<TagUsage> {
    const storedSession = await readStoredSession();
    const usage = await this.request<BackendTagUsageRead>(
      `/api/v1/inventory/tags/${normalizeTagUid(input.tagUid)}/shipments/full`,
      {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({
          note: input.note?.trim() || null,
        }),
      },
      storedSession?.accessToken
    );

    return toTagUsage(usage);
  }

  async getActiveTags(productId?: number): Promise<ActiveTag[]> {
    const activeTags = await this.getAuthorized<BackendActiveTagRead[]>('/api/v1/inventory/tags/active');
    const mappedTags = activeTags.map(toActiveTag);

    if (productId === undefined) {
      return mappedTags;
    }

    return mappedTags.filter((tag) => tag.productId === productId);
  }

  async getTagHistory(tagUid: string): Promise<TagHistory | null> {
    try {
      const history = await this.getAuthorized<BackendTagHistoryRead>(
        `/api/v1/inventory/tags/${normalizeTagUid(tagUid)}/history`
      );

      return {
        tagUid: normalizeTagUid(history.tag_uid),
        usages: history.usages.map(toTagUsage),
      };
    } catch (error) {
      if (error instanceof ApiError && error.status === 404) {
        return null;
      }

      throw error;
    }
  }

  async lookupTag(tagUid: string): Promise<TagLookupResult | null> {
    const history = await this.getTagHistory(tagUid);

    if (!history) {
      return null;
    }

    return {
      tagUid: history.tagUid,
      activeUsage: history.usages.find((usage) => usage.closedAt === null) ?? null,
      usages: history.usages,
    };
  }
}
