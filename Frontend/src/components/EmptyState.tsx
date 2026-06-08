import { StyleSheet, Text, View } from 'react-native';

import { AppCard } from '@/src/components/AppCard';
import { colors, radius, spacing } from '@/src/theme';

type EmptyStateProps = {
  title: string;
  description: string;
};

export function EmptyState({ title, description }: EmptyStateProps) {
  return (
    <AppCard>
      <View style={styles.iconCircle}>
        <View style={styles.iconInner} />
      </View>
      <Text style={styles.title}>{title}</Text>
      <Text style={styles.description}>{description}</Text>
    </AppCard>
  );
}

const styles = StyleSheet.create({
  iconCircle: {
    alignItems: 'center',
    backgroundColor: colors.primarySoft,
    borderRadius: radius.pill,
    height: 44,
    justifyContent: 'center',
    width: 44,
  },
  iconInner: {
    backgroundColor: colors.primary,
    borderRadius: radius.pill,
    height: 14,
    opacity: 0.6,
    width: 14,
  },
  title: {
    color: colors.text,
    fontSize: 17,
    fontWeight: '700',
  },
  description: {
    color: colors.textMuted,
    fontSize: 14,
    lineHeight: 21,
  },
});
