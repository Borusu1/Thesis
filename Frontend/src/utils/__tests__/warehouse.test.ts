import { resolveProductStatus } from '@/src/utils/warehouse';

describe('warehouse utils', () => {
  describe('resolveProductStatus', () => {
    it('reports inStock for a positive quantity', () => {
      expect(resolveProductStatus(1)).toBe('inStock');
      expect(resolveProductStatus(999)).toBe('inStock');
    });

    it('reports outOfStock for zero quantity', () => {
      expect(resolveProductStatus(0)).toBe('outOfStock');
    });

    it('reports outOfStock for a negative quantity', () => {
      expect(resolveProductStatus(-5)).toBe('outOfStock');
    });
  });
});
