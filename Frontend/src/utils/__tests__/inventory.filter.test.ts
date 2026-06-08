import { filterProducts } from '@/src/utils/inventory';
import { Product } from '@/src/types/warehouse';

const products: Product[] = [
  {
    id: 1,
    sku: 1001,
    name: 'Яблука',
    description: 'Палета яблук',
    quantityOnHand: 12,
    createdAt: '2026-03-24T08:00:00.000Z',
    status: 'inStock',
  },
  {
    id: 2,
    sku: 1002,
    name: 'Абрикоси',
    description: 'Літня партія',
    quantityOnHand: 0,
    createdAt: '2026-03-24T09:00:00.000Z',
    status: 'outOfStock',
  },
  {
    id: 42,
    sku: 1003,
    name: 'Банани',
    description: null,
    quantityOnHand: 5,
    createdAt: '2026-03-25T09:00:00.000Z',
    status: 'inStock',
  },
];

describe('filterProducts', () => {
  it('returns all products when search is empty and filter is all', () => {
    expect(filterProducts(products, '', 'all')).toHaveLength(3);
  });

  it('filters by inStock status', () => {
    const result = filterProducts(products, '', 'inStock');

    expect(result.map((product) => product.id)).toEqual([1, 42]);
  });

  it('filters by outOfStock status', () => {
    const result = filterProducts(products, '', 'outOfStock');

    expect(result).toHaveLength(1);
    expect(result[0].id).toBe(2);
  });

  it('matches by product name case-insensitively', () => {
    const result = filterProducts(products, 'ЯБЛУКА', 'all');

    expect(result).toHaveLength(1);
    expect(result[0].id).toBe(1);
  });

  it('matches by description', () => {
    const result = filterProducts(products, 'літня', 'all');

    expect(result).toHaveLength(1);
    expect(result[0].id).toBe(2);
  });

  it('matches by id', () => {
    const result = filterProducts(products, '42', 'all');

    expect(result).toHaveLength(1);
    expect(result[0].id).toBe(42);
  });

  it('tolerates products with a null description', () => {
    const result = filterProducts(products, 'банани', 'all');

    expect(result).toHaveLength(1);
    expect(result[0].id).toBe(42);
  });

  it('combines status and search filters', () => {
    expect(filterProducts(products, 'яблука', 'outOfStock')).toHaveLength(0);
  });

  it('returns an empty array when nothing matches', () => {
    expect(filterProducts(products, 'неіснуючий', 'all')).toEqual([]);
  });
});
