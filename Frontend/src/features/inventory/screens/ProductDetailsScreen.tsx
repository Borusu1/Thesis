import { useEffect, useState } from 'react';
import { StyleSheet, Text, View } from 'react-native';
import { useLocalSearchParams } from 'expo-router';
import { useIsFocused } from '@react-navigation/native';

import { AppButton } from '@/src/components/AppButton';
import { AppCard } from '@/src/components/AppCard';
import { AppInput } from '@/src/components/AppInput';
import { AppScreen } from '@/src/components/AppScreen';
import { EmptyState } from '@/src/components/EmptyState';
import { StatusBadge } from '@/src/components/StatusBadge';
import { OperationListItem } from '@/src/features/history/components/OperationListItem';
import { ProductDetailsSkeleton } from '@/src/features/inventory/components/ProductDetailsSkeleton';
import { useI18n } from '@/src/providers/LocaleProvider';
import { useWarehouseService } from '@/src/providers/WarehouseServiceProvider';
import { colors, spacing } from '@/src/theme';
import { ActiveTag, Operation, Product } from '@/src/types/warehouse';
import { formatDateTime, formatQuantity } from '@/src/utils/format';
import { validatePositiveIntegerInput } from '@/src/utils/forms';

export function ProductDetailsScreen() {
  const { productId } = useLocalSearchParams<{ productId: string }>();
  const { locale, t } = useI18n();
  const warehouseService = useWarehouseService();
  const isFocused = useIsFocused();
  const [product, setProduct] = useState<Product | null>(null);
  const [operations, setOperations] = useState<Operation[]>([]);
  const [activeTags, setActiveTags] = useState<ActiveTag[]>([]);
  const [isLoading, setIsLoading] = useState(true);
  const [partialQuantities, setPartialQuantities] = useState<Record<string, string>>({});
  const [partialNotes, setPartialNotes] = useState<Record<string, string>>({});
  const [formError, setFormError] = useState<string | null>(null);
  const [activeSubmissionKey, setActiveSubmissionKey] = useState<string | null>(null);

  useEffect(() => {
    let isMounted = true;

    async function load() {
      if (!isFocused || !productId) {
        return;
      }

      setIsLoading(true);

      try {
        const numericProductId = Number(productId);
        const [nextProduct, nextOperations, nextActiveTags] = await Promise.all([
          warehouseService.getProductById(numericProductId),
          warehouseService.getOperations({ productId: numericProductId }),
          warehouseService.getActiveTags(numericProductId),
        ]);

        if (isMounted) {
          setProduct(nextProduct);
          setOperations(nextOperations);
          setActiveTags(nextActiveTags);
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
  }, [isFocused, productId, warehouseService]);

  async function reload() {
    if (!productId) {
      return;
    }

    const numericProductId = Number(productId);
    const [nextProduct, nextOperations, nextActiveTags] = await Promise.all([
      warehouseService.getProductById(numericProductId),
      warehouseService.getOperations({ productId: numericProductId }),
      warehouseService.getActiveTags(numericProductId),
    ]);

    setProduct(nextProduct);
    setOperations(nextOperations);
    setActiveTags(nextActiveTags);
  }

  async function handlePartialShipment(tagUid: string) {
    const quantity = partialQuantities[tagUid] ?? '1';

    if (!validatePositiveIntegerInput(quantity)) {
      setFormError(t('validationPositiveInteger'));
      return;
    }

    setFormError(null);
    setActiveSubmissionKey(`partial:${tagUid}`);

    try {
      await warehouseService.createPartialShipment({
        tagUid,
        quantity: Number(quantity),
        note: partialNotes[tagUid],
      });
      setPartialQuantities((current) => ({ ...current, [tagUid]: '1' }));
      setPartialNotes((current) => ({ ...current, [tagUid]: '' }));
      await reload();
    } catch (error) {
      setFormError(error instanceof Error ? error.message : t('genericError'));
    } finally {
      setActiveSubmissionKey(null);
    }
  }

  async function handleFullShipment(tagUid: string) {
    setFormError(null);
    setActiveSubmissionKey(`full:${tagUid}`);

    try {
      await warehouseService.createFullShipment({
        tagUid,
        note: partialNotes[tagUid],
      });
      setPartialQuantities((current) => ({ ...current, [tagUid]: '1' }));
      setPartialNotes((current) => ({ ...current, [tagUid]: '' }));
      await reload();
    } catch (error) {
      setFormError(error instanceof Error ? error.message : t('genericError'));
    } finally {
      setActiveSubmissionKey(null);
    }
  }

  if (isLoading) {
    return (
      <AppScreen subtitle={t('productLoadingSubtitle')} title={t('productLoadingTitle')}>
        <ProductDetailsSkeleton />
      </AppScreen>
    );
  }

  if (!product) {
    return (
      <AppScreen subtitle={t('productMissingSubtitle')} title={t('productMissingTitle')}>
        <EmptyState description={t('productMissingDescription')} title={t('productMissingTitle')} />
      </AppScreen>
    );
  }

  return (
    <AppScreen subtitle={`#${product.id}`} title={product.name}>
      <AppCard>
        <View style={styles.header}>
          <Text style={styles.quantity}>{formatQuantity(product.quantityOnHand)}</Text>
          <StatusBadge status={product.status} />
        </View>
        <View style={styles.summaryRow}>
          <Text style={styles.summaryLabel}>{t('fieldDescription')}</Text>
          <Text style={styles.summaryValue}>{product.description || t('productNoDescription')}</Text>
        </View>
        <View style={styles.summaryRow}>
          <Text style={styles.summaryLabel}>{t('inventoryCreatedLabel')}</Text>
          <Text style={styles.summaryValue}>{formatDateTime(product.createdAt, locale)}</Text>
        </View>
      </AppCard>

      <View style={styles.historySection}>
        <Text style={styles.sectionTitle}>{t('productTagsTitle')}</Text>
        {activeTags.length ? (
          activeTags.map((tag) => (
            <AppCard key={tag.tagUid}>
              <View style={styles.tagHeader}>
                <View style={styles.tagHeaderText}>
                  <Text style={styles.tagId}>{tag.tagUid}</Text>
                  <Text style={styles.helperText}>
                    {formatQuantity(tag.quantityCurrent)} • {t('inventoryReceivedLabel')} {formatDateTime(tag.arrivedAt, locale)}
                  </Text>
                  {tag.warehouseLocation ? (
                    <Text style={styles.helperText}>
                      {t('inventoryLocationLabel')}: {tag.warehouseLocation}
                    </Text>
                  ) : null}
                </View>
              </View>
              <AppInput
                keyboardType="number-pad"
                label={t('partialShipmentQuantityLabel')}
                onChangeText={(value) =>
                  setPartialQuantities((current) => ({
                    ...current,
                    [tag.tagUid]: value,
                  }))
                }
                placeholder="1"
                value={partialQuantities[tag.tagUid] ?? '1'}
              />
              <AppInput
                label={t('fieldComment')}
                multiline
                numberOfLines={3}
                onChangeText={(value) =>
                  setPartialNotes((current) => ({
                    ...current,
                    [tag.tagUid]: value,
                  }))
                }
                placeholder={t('fieldCommentPlaceholder')}
                value={partialNotes[tag.tagUid] ?? ''}
              />
              {formError ? <Text style={styles.errorText}>{formError}</Text> : null}
              <View style={styles.actionsRow}>
                <AppButton
                  disabled={activeSubmissionKey === `partial:${tag.tagUid}`}
                  label={t('partialShipmentAction')}
                  onPress={() => handlePartialShipment(tag.tagUid)}
                />
                <AppButton
                  disabled={activeSubmissionKey === `full:${tag.tagUid}`}
                  label={t('fullShipmentAction')}
                  onPress={() => handleFullShipment(tag.tagUid)}
                  variant="secondary"
                />
              </View>
            </AppCard>
          ))
        ) : (
          <EmptyState description={t('productNoTags')} title={t('productTagsEmptyTitle')} />
        )}
      </View>

      <View style={styles.historySection}>
        <Text style={styles.sectionTitle}>{t('productHistoryTitle')}</Text>
        {operations.length ? (
          operations.map((operation) => (
            <OperationListItem
              key={operation.id}
              operation={operation}
              productName={product.name}
            />
          ))
        ) : (
          <EmptyState description={t('productHistoryEmptyDescription')} title={t('productHistoryEmptyTitle')} />
        )}
      </View>
    </AppScreen>
  );
}

const styles = StyleSheet.create({
  header: {
    alignItems: 'flex-start',
    flexDirection: 'row',
    justifyContent: 'space-between',
  },
  quantity: {
    color: colors.text,
    fontSize: 24,
    fontWeight: '700',
  },
  summaryRow: {
    gap: spacing.xs,
  },
  summaryLabel: {
    color: colors.textMuted,
    fontSize: 12,
    fontWeight: '600',
  },
  summaryValue: {
    color: colors.text,
    fontSize: 15,
  },
  sectionTitle: {
    color: colors.text,
    fontSize: 17,
    fontWeight: '700',
  },
  historySection: {
    gap: spacing.md,
  },
  tagHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
  },
  tagHeaderText: {
    gap: spacing.xs,
  },
  tagId: {
    color: colors.text,
    fontSize: 15,
    fontWeight: '700',
  },
  helperText: {
    color: colors.textMuted,
    fontSize: 13,
    lineHeight: 20,
  },
  errorText: {
    color: colors.danger,
    fontSize: 14,
    lineHeight: 20,
  },
  actionsRow: {
    flexDirection: 'row',
    gap: spacing.md,
  },
});
