import { StyleSheet, Text, View } from 'react-native';

import { AppCard } from '@/src/components/AppCard';
import { colors, spacing } from '@/src/theme';

type MetricCardProps = {
  label: string;
  value: string;
  tone?: 'default' | 'success' | 'warning' | 'danger';
};

const toneMap = {
  default: colors.primary,
  success: colors.success,
  warning: colors.warning,
  danger: colors.danger,
} as const;

export function MetricCard({ label, value, tone = 'default' }: MetricCardProps) {
  return (
    <View style={styles.wrapper}>
      <AppCard>
        <Text style={styles.label}>{label}</Text>
        <Text style={[styles.value, { color: toneMap[tone] }]}>{value}</Text>
      </AppCard>
    </View>
  );
}

const styles = StyleSheet.create({
  wrapper: {
    minWidth: '48%',
  },
  label: {
    color: colors.textMuted,
    fontSize: 13,
    fontWeight: '600',
  },
  value: {
    fontSize: 28,
    fontWeight: '700',
  },
});
