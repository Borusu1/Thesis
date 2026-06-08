import { useEffect, useMemo, useState } from 'react';
import { Pressable, StyleSheet, Text, TextInput, View } from 'react-native';
import { router } from 'expo-router';
import { useIsFocused } from '@react-navigation/native';

import { EmptyState } from '@/src/components/EmptyState';
import { AppScreen } from '@/src/components/AppScreen';
import { InventorySkeleton } from '@/src/features/inventory/components/InventorySkeleton';
import { ProductListItem } from '@/src/features/inventory/components/ProductListItem';
import { useI18n } from '@/src/providers/LocaleProvider';
import { useWarehouseService } from '@/src/providers/WarehouseServiceProvider';
import { colors, radius, spacing } from '@/src/theme';
import { Product } from '@/src/types/warehouse';
import { filterProducts, InventoryFilter } from '@/src/utils/inventory';

const filters: InventoryFilter[] = ['all', 'inStock', 'outOfStock'];

export function InventoryScreen() {
  const { t } = useI18n();
  const warehouseService = useWarehouseService();
  const isFocused = useIsFocused();
  const [products, setProducts] = useState<Product[]>([]);
  const [isLoading, setIsLoading] = useState(true);
  const [search, setSearch] = useState('');
  const [activeFilter, setActiveFilter] = useState<InventoryFilter>('all');

  useEffect(() => {
    let isMounted = true;

    async function load() {
      if (!isFocused) {
        return;
      }

      setIsLoading(true);

      try {
        const nextProducts = await warehouseService.getProducts();

        if (isMounted) {
          setProducts(nextProducts);
        }
      } finally {
        if (isMounted) {
          setIsLoading(false);
        }
      }
    }

    load();

    return () => {
      isMounted = false;
    };
  }, [isFocused, warehouseService]);

  const filteredProducts = useMemo(
    () => filterProducts(products, search, activeFilter),
    [activeFilter, products, search]
  );

  return (
    <AppScreen subtitle={t('inventorySubtitle')} title={t('inventoryTitle')}>
      <TextInput
        onChangeText={setSearch}
        placeholder={t('inventorySearchPlaceholder')}
        placeholderTextColor={colors.textMuted}
        style={styles.searchInput}
        value={search}
      />

      <View style={styles.filterRow}>
        {filters.map((filter) => {
          const isActive = activeFilter === filter;
          const label = filter === 'all' ? t('filterAll') : t(`status.${filter}`);

          return (
            <Pressable
              key={filter}
              onPress={() => setActiveFilter(filter)}
              style={[styles.filterChip, isActive ? styles.activeChip : styles.inactiveChip]}
            >
              <Text style={[styles.filterLabel, isActive ? styles.activeLabel : styles.inactiveLabel]}>
                {label}
              </Text>
            </Pressable>
          );
        })}
      </View>

      {isLoading ? (
        <InventorySkeleton />
      ) : filteredProducts.length ? (
        filteredProducts.map((product) => (
          <ProductListItem
            key={product.id}
            onPress={() =>
              router.push({
                pathname: '/(app)/product/[productId]',
                params: { productId: String(product.id) },
              })
            }
            product={product}
          />
        ))
      ) : (
        <EmptyState description={t('inventoryEmptyDescription')} title={t('inventoryEmptyTitle')} />
      )}
    </AppScreen>
  );
}

const styles = StyleSheet.create({
  searchInput: {
    backgroundColor: colors.surface,
    borderColor: colors.border,
    borderRadius: radius.md,
    borderWidth: 1,
    color: colors.text,
    fontSize: 15,
    minHeight: 48,
    paddingHorizontal: spacing.md,
    paddingVertical: spacing.md,
  },
  filterRow: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    gap: spacing.sm,
  },
  filterChip: {
    borderRadius: radius.pill,
    paddingHorizontal: spacing.md,
    paddingVertical: spacing.sm,
  },
  activeChip: {
    backgroundColor: colors.primary,
  },
  inactiveChip: {
    backgroundColor: colors.primarySoft,
  },
  filterLabel: {
    fontSize: 13,
    fontWeight: '700',
  },
  activeLabel: {
    color: colors.surface,
  },
  inactiveLabel: {
    color: colors.primary,
  },
});
