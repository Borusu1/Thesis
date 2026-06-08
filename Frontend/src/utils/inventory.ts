import { Product, ProductStatus } from '@/src/types/warehouse';
import { normalizeTagUid } from '@/src/utils/tag';

export type InventoryFilter = 'all' | ProductStatus;

export function filterProducts(products: Product[], search: string, filter: InventoryFilter) {
  const normalizedSearch = normalizeTagUid(search);

  return products.filter((product) => {
    const matchesFilter = filter === 'all' ? true : product.status === filter;
    const matchesSearch =
      !normalizedSearch ||
      normalizeTagUid(product.name).includes(normalizedSearch) ||
      normalizeTagUid(product.description ?? '').includes(normalizedSearch) ||
      String(product.id).includes(normalizedSearch);

    return matchesFilter && matchesSearch;
  });
}
