import {
  formatOperationTypeLabel,
  formatQuantity,
  formatSignedQuantity,
  resolveOperationDelta,
} from '@/src/utils/format';
import { Operation } from '@/src/types/warehouse';

function buildOperation(overrides: Partial<Operation> = {}): Operation {
  return {
    id: 1,
    usageId: 10,
    productId: 1,
    productNameSnapshot: 'Яблука',
    type: 'receipt',
    quantity: 5,
    quantityDelta: 5,
    note: '',
    actor: 'API',
    createdAt: '2026-03-22T09:00:00.000Z',
    tagUid: '123e4567-e89b-12d3-a456-426614174000',
    ...overrides,
  };
}

describe('format utils', () => {
  describe('formatQuantity', () => {
    it('uses the default unit', () => {
      expect(formatQuantity(12)).toBe('12 шт');
    });

    it('supports a custom unit', () => {
      expect(formatQuantity(3, 'кг')).toBe('3 кг');
    });

    it('formats zero quantity', () => {
      expect(formatQuantity(0)).toBe('0 шт');
    });
  });

  describe('formatSignedQuantity', () => {
    it('prefixes positive numbers with a plus sign', () => {
      expect(formatSignedQuantity(7)).toBe('+7 шт');
    });

    it('keeps the native minus for negative numbers', () => {
      expect(formatSignedQuantity(-4)).toBe('-4 шт');
    });

    it('does not add a sign for zero', () => {
      expect(formatSignedQuantity(0)).toBe('0 шт');
    });

    it('supports a custom unit', () => {
      expect(formatSignedQuantity(2, 'кг')).toBe('+2 кг');
    });
  });

  describe('resolveOperationDelta', () => {
    it('returns a positive delta for receipts', () => {
      expect(resolveOperationDelta(buildOperation({ type: 'receipt', quantity: 8 }))).toBe(8);
    });

    it('returns a negative delta for shipments', () => {
      expect(
        resolveOperationDelta(buildOperation({ type: 'shipment_partial', quantity: 8 }))
      ).toBe(-8);
    });
  });

  describe('formatOperationTypeLabel', () => {
    it('delegates to the translator with a namespaced key', () => {
      const t = jest.fn((key: string) => `translated:${key}`);

      expect(formatOperationTypeLabel('receipt', t)).toBe('translated:operationType.receipt');
      expect(t).toHaveBeenCalledWith('operationType.receipt');
    });
  });

  describe('formatDateTime', () => {
    it('formats an ISO timestamp into a localized day/month/year string', () => {
      const { formatDateTime } = require('@/src/utils/format');
      const formatted = formatDateTime('2026-03-22T09:05:00.000Z', 'uk');

      // The exact separators depend on the ICU build, so assert on the parts.
      expect(formatted).toMatch(/2026/);
      expect(formatted).toMatch(/03/);
    });
  });
});
