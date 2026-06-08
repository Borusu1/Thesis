import { StyleSheet, Text, View } from 'react-native';

import { useI18n } from '@/src/providers/LocaleProvider';
import { colors, radius, spacing } from '@/src/theme';
import { ProductStatus } from '@/src/types/warehouse';

type StatusBadgeProps = {
  status: ProductStatus;
};

const statusColors = {
  inStock: {
    backgroundColor: '#def7ec',
    textColor: colors.success,
  },
  outOfStock: {
    backgroundColor: '#fee4e2',
    textColor: colors.danger,
  },
} as const;

export function StatusBadge({ status }: StatusBadgeProps) {
  const { t } = useI18n();
  const palette = statusColors[status];

  return (
    <View style={[styles.badge, { backgroundColor: palette.backgroundColor }]}>
      <Text style={[styles.label, { color: palette.textColor }]}>{t(`status.${status}`)}</Text>
    </View>
  );
}

const styles = StyleSheet.create({
  badge: {
    alignSelf: 'flex-start',
    borderRadius: radius.pill,
    paddingHorizontal: spacing.md,
    paddingVertical: spacing.sm,
  },
  label: {
    fontSize: 12,
    fontWeight: '700',
    letterSpacing: 0.3,
    textTransform: 'uppercase',
  },
});
