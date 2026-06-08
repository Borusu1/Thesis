import { Pressable, StyleSheet, Text, View } from 'react-native';

import { AppCard } from '@/src/components/AppCard';
import { useI18n } from '@/src/providers/LocaleProvider';
import { colors, spacing } from '@/src/theme';
import { Operation } from '@/src/types/warehouse';
import { formatDateTime, formatOperationTypeLabel, formatSignedQuantity } from '@/src/utils/format';

type OperationListItemProps = {
  operation: Operation;
  productName: string;
  onPress?: () => void;
};

export function OperationListItem({
  operation,
  productName,
  onPress,
}: OperationListItemProps) {
  const { locale, t } = useI18n();

  const content = (
    <AppCard>
      <View style={styles.header}>
        <View style={styles.headerText}>
          <Text style={styles.productName}>{productName}</Text>
          <Text style={styles.type}>{formatOperationTypeLabel(operation.type, t)}</Text>
        </View>
        <Text style={styles.delta}>{formatSignedQuantity(operation.quantityDelta)}</Text>
      </View>
      <Text style={styles.note}>{operation.note || t('operationNoComment')}</Text>
      <View style={styles.metaRow}>
        <Text style={styles.meta}>{formatDateTime(operation.createdAt, locale)}</Text>
        <Text style={styles.meta}>{operation.tagUid}</Text>
      </View>
    </AppCard>
  );

  if (!onPress) {
    return content;
  }

  return (
    <Pressable accessibilityRole="button" onPress={onPress}>
      {content}
    </Pressable>
  );
}

const styles = StyleSheet.create({
  header: {
    alignItems: 'flex-start',
    flexDirection: 'row',
    gap: spacing.md,
    justifyContent: 'space-between',
  },
  headerText: {
    flex: 1,
    gap: spacing.xs,
  },
  productName: {
    color: colors.text,
    fontSize: 16,
    fontWeight: '700',
  },
  type: {
    color: colors.primary,
    fontSize: 13,
    fontWeight: '600',
  },
  delta: {
    color: colors.text,
    fontSize: 15,
    fontWeight: '700',
  },
  note: {
    color: colors.textMuted,
    fontSize: 14,
    lineHeight: 21,
  },
  metaRow: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    gap: spacing.md,
  },
  meta: {
    color: colors.textMuted,
    fontSize: 12,
    fontWeight: '600',
  },
});
