import { useEffect, useState } from 'react';
import { StyleSheet, Text, View } from 'react-native';
import { useIsFocused } from '@react-navigation/native';

import { EmptyState } from '@/src/components/EmptyState';
import { MetricCard } from '@/src/components/MetricCard';
import { AppScreen } from '@/src/components/AppScreen';
import { DashboardSkeleton } from '@/src/features/dashboard/components/DashboardSkeleton';
import { useI18n } from '@/src/providers/LocaleProvider';
import { useWarehouseService } from '@/src/providers/WarehouseServiceProvider';
import { colors, spacing } from '@/src/theme';
import { DashboardSummary, Product } from '@/src/types/warehouse';
import { formatQuantity } from '@/src/utils/format';

export function DashboardScreen() {
  const { t } = useI18n();
  const warehouseService = useWarehouseService();
  const isFocused = useIsFocused();
  const [summary, setSummary] = useState<DashboardSummary | null>(null);
  const [products, setProducts] = useState<Product[]>([]);
  const [isLoading, setIsLoading] = useState(true);

  useEffect(() => {
    let isMounted = true;

    async function load() {
      if (!isFocused) {
        return;
      }

      setIsLoading(true);

      try {
        const [nextSummary, nextProducts] = await Promise.all([
          warehouseService.getDashboardSummary(),
          warehouseService.getProducts(),
        ]);

        if (isMounted) {
          setSummary(nextSummary);
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

  const attentionProducts = products.filter((product) => product.status === 'outOfStock').slice(0, 3);

  return (
    <AppScreen subtitle={t('dashboardSubtitle')} title={t('dashboardTitle')}>
      {isLoading || !summary ? (
        <DashboardSkeleton />
      ) : (
        <>
          <View style={styles.metricGrid}>
            <MetricCard label={t('dashboardMetricProducts')} value={String(summary.totalProducts)} />
            <MetricCard label={t('dashboardMetricUnits')} tone="success" value={String(summary.totalUnits)} />
            <MetricCard label={t('dashboardMetricOutOfStock')} tone="danger" value={String(summary.outOfStockCount)} />
          </View>

          <View style={styles.section}>
            <Text style={styles.sectionTitle}>{t('dashboardAttentionTitle')}</Text>
            {attentionProducts.length ? (
              attentionProducts.map((product) => (
                <View key={product.id} style={styles.infoCard}>
                  <Text style={styles.infoValue}>{product.name}</Text>
                  <Text style={styles.infoCaption}>{formatQuantity(product.quantityOnHand)}</Text>
                </View>
              ))
            ) : (
              <EmptyState
                description={t('dashboardAttentionEmptyDescription')}
                title={t('dashboardAttentionEmptyTitle')}
              />
            )}
          </View>

          <View style={styles.section}>
            <Text style={styles.sectionTitle}>{t('dashboardRecentActionsTitle')}</Text>
            {summary.recentOperations.length ? (
              summary.recentOperations.map((operation) => (
                <View key={operation.id} style={styles.infoCard}>
                  <Text style={styles.infoValue}>{operation.productNameSnapshot}</Text>
                  <Text style={styles.infoCaption}>{operation.note || t('operationNoComment')}</Text>
                </View>
              ))
            ) : (
              <EmptyState description={t('historyEmptyDescription')} title={t('historyEmptyTitle')} />
            )}
          </View>
        </>
      )}
    </AppScreen>
  );
}

const styles = StyleSheet.create({
  metricGrid: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    gap: spacing.md,
  },
  section: {
    gap: spacing.md,
  },
  sectionTitle: {
    color: colors.text,
    fontSize: 17,
    fontWeight: '700',
  },
  infoCard: {
    backgroundColor: colors.surface,
    borderColor: colors.border,
    borderRadius: 16,
    borderWidth: 1,
    gap: spacing.xs,
    padding: spacing.lg,
  },
  infoValue: {
    color: colors.text,
    fontSize: 15,
    fontWeight: '700',
  },
  infoCaption: {
    color: colors.textMuted,
    fontSize: 14,
    lineHeight: 20,
  },
});
