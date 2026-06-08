import { ProductStatus } from '@/src/types/warehouse';

export function resolveProductStatus(quantityOnHand: number): ProductStatus {
  return quantityOnHand > 0 ? 'inStock' : 'outOfStock';
}
