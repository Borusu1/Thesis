import { useEffect, useMemo, useState } from 'react';
import { Pressable, StyleSheet, Text, View } from 'react-native';
import { router } from 'expo-router';
import { useIsFocused } from '@react-navigation/native';

import { EmptyState } from '@/src/components/EmptyState';
import { AppScreen } from '@/src/components/AppScreen';
import { HistorySkeleton } from '@/src/features/history/components/HistorySkeleton';
import { OperationListItem } from '@/src/features/history/components/OperationListItem';
import { useI18n } from '@/src/providers/LocaleProvider';
import { useWarehouseService } from '@/src/providers/WarehouseServiceProvider';
import { colors, radius, spacing } from '@/src/theme';
import { Operation, OperationType } from '@/src/types/warehouse';

type OperationFilter = 'all' | OperationType;

const operationFilters: OperationFilter[] = ['all', 'receipt', 'shipment_partial', 'shipment_full'];

export function HistoryScreen() {
  const { t } = useI18n();
  const warehouseService = useWarehouseService();
  const isFocused = useIsFocused();
  const [operations, setOperations] = useState<Operation[]>([]);
  const [activeFilter, setActiveFilter] = useState<OperationFilter>('all');
  const [isLoading, setIsLoading] = useState(true);

  useEffect(() => {
    let isMounted = true;

    async function load() {
      if (!isFocused) {
        return;
      }

      setIsLoading(true);

      try {
        const nextOperations = await warehouseService.getOperations();

        if (isMounted) {
          setOperations(nextOperations);
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

  const filteredOperations = useMemo(
    () => operations.filter((operation) => activeFilter === 'all' || operation.type === activeFilter),
    [activeFilter, operations]
  );

  return (
    <AppScreen subtitle={t('historySubtitle')} title={t('historyTitle')}>
      <View style={styles.filterRow}>
        {operationFilters.map((filter) => {
          const isActive = activeFilter === filter;
          const label = filter === 'all' ? t('filterAll') : t(`operationType.${filter}`);

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
        <HistorySkeleton />
      ) : filteredOperations.length ? (
        filteredOperations.map((operation) => (
          <OperationListItem
            key={operation.id}
            onPress={() =>
              router.push({
                pathname: '/(app)/product/[productId]',
                params: { productId: String(operation.productId) },
              })
            }
            operation={operation}
            productName={operation.productNameSnapshot}
          />
        ))
      ) : (
        <EmptyState description={t('historyEmptyDescription')} title={t('historyEmptyTitle')} />
      )}
    </AppScreen>
  );
}

const styles = StyleSheet.create({
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
