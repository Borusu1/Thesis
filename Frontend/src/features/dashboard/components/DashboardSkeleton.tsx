import { StyleSheet, View } from 'react-native';

import { AppCard } from '@/src/components/AppCard';
import { SkeletonBlock } from '@/src/components/SkeletonBlock';
import { SkeletonText } from '@/src/components/SkeletonText';
import { spacing } from '@/src/theme';

export function DashboardSkeleton() {
  return (
    <View style={styles.wrapper}>
      <View style={styles.metricGrid}>
        {[0, 1, 2, 3].map((item) => (
          <View key={item} style={styles.metricItem}>
            <AppCard>
              <SkeletonText width="55%" />
              <SkeletonBlock height={28} width="45%" />
            </AppCard>
          </View>
        ))}
      </View>
      {[0, 1, 2].map((item) => (
        <AppCard key={item}>
          <SkeletonText width="40%" />
          <SkeletonText width="80%" />
          <SkeletonText width="60%" />
        </AppCard>
      ))}
    </View>
  );
}

const styles = StyleSheet.create({
  wrapper: {
    gap: spacing.lg,
  },
  metricGrid: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    gap: spacing.md,
  },
  metricItem: {
    minWidth: '48%',
  },
});
