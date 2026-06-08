import AsyncStorage from '@react-native-async-storage/async-storage';

import { ApiWarehouseDataService } from '@/src/services/warehouse/ApiWarehouseDataService';

function createJsonResponse(body: unknown, init: ResponseInit = {}) {
  return new Response(JSON.stringify(body), {
    status: 200,
    headers: { 'Content-Type': 'application/json' },
    ...init,
  });
}

describe('ApiWarehouseDataService', () => {
  const originalFetch = global.fetch;

  afterEach(() => {
    global.fetch = originalFetch;
  });

  it('stores session after login and refreshes it from /auth/me', async () => {
    const service = new ApiWarehouseDataService();
    global.fetch = jest
      .fn()
      .mockResolvedValueOnce(
        createJsonResponse({
          access_token: 'token-1',
          token_type: 'bearer',
        })
      )
      .mockResolvedValueOnce(
        createJsonResponse({
          id: 1,
          email: 'demo@example.com',
          role: 'admin',
          is_active: true,
          created_at: '2026-03-24T10:00:00Z',
        })
      )
      .mockResolvedValueOnce(
        createJsonResponse({
          id: 1,
          email: 'demo@example.com',
          role: 'admin',
          is_active: true,
          created_at: '2026-03-24T10:00:00Z',
        })
      ) as typeof fetch;

    const session = await service.login('demo@example.com', 'demo123');

    expect(session.email).toBe('demo@example.com');
    expect((await AsyncStorage.getItem('warehouse.session')) ?? '').toContain('token-1');

    const restoredSession = await service.getSession();
    expect(restoredSession?.accessToken).toBe('token-1');
  });

  it('maps products and inventory events from backend responses', async () => {
    const service = new ApiWarehouseDataService();
    await AsyncStorage.setItem(
      'warehouse.session',
      JSON.stringify({
        accessToken: 'token-1',
        user: {
          id: 1,
          email: 'demo@example.com',
          role: 'admin',
          is_active: true,
          created_at: '2026-03-24T10:00:00Z',
        },
      })
    );

    global.fetch = jest
      .fn()
      .mockResolvedValueOnce(
        createJsonResponse([
          {
            id: 1,
            sku: 1001,
            name: 'Яблука',
            description: 'Палета яблук',
            quantity_on_hand: 12,
            created_at: '2026-03-24T08:00:00Z',
          },
        ])
      )
      .mockResolvedValueOnce(
        createJsonResponse([
          {
            id: 11,
            usage_id: 4,
            product_id: 1,
            product_name_snapshot: 'Яблука',
            tag_uid: '123e4567-e89b-12d3-a456-426614174000',
            event_type: 'shipment_partial',
            quantity: 3,
            occurred_at: '2026-03-24T09:00:00Z',
            note: 'Partial',
          },
        ])
      ) as typeof fetch;

    const [products, operations] = await Promise.all([service.getProducts(), service.getOperations()]);

    expect(products[0]).toMatchObject({
      id: 1,
      sku: 1001,
      name: 'Яблука',
      quantityOnHand: 12,
      status: 'inStock',
    });
    expect(operations[0]).toMatchObject({
      productId: 1,
      type: 'shipment_partial',
      quantityDelta: -3,
    });
  });
});
