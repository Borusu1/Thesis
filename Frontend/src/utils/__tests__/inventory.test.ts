import { filterProducts } from '@/src/utils/inventory';
import { extractTagUid, isValidTagUid, normalizeTagUid } from '@/src/utils/tag';
import { resolveProductStatus } from '@/src/utils/warehouse';

describe('inventory utils', () => {
  const products = [
    {
      id: 1,
      sku: 1001,
      name: 'Яблука',
      description: 'Палета яблук',
      quantityOnHand: 12,
      createdAt: '2026-03-24T08:00:00.000Z',
      status: 'inStock' as const,
    },
    {
      id: 2,
      sku: 1002,
      name: 'Абрикоси',
      description: 'Літня партія',
      quantityOnHand: 0,
      createdAt: '2026-03-24T09:00:00.000Z',
      status: 'outOfStock' as const,
    },
  ];

  it('filters products by status and text search', () => {
    const filtered = filterProducts(products, 'літня', 'outOfStock');

    expect(filtered).toHaveLength(1);
    expect(filtered[0].id).toBe(2);
  });

  it('normalizes and validates tag UUIDs', () => {
    expect(normalizeTagUid(' 123E4567-E89B-12D3-A456-426614174000 ')).toBe(
      '123e4567-e89b-12d3-a456-426614174000'
    );
    expect(extractTagUid('tag: 123e4567-e89b-12d3-a456-426614174000')).toBe(
      '123e4567-e89b-12d3-a456-426614174000'
    );
    expect(isValidTagUid('123e4567-e89b-12d3-a456-426614174000')).toBe(true);
  });

  it('resolves product status from quantity', () => {
    expect(resolveProductStatus(0)).toBe('outOfStock');
    expect(resolveProductStatus(10)).toBe('inStock');
  });
});
