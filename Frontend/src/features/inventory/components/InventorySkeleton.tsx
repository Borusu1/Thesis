import { View } from 'react-native';

import { AppCard } from '@/src/components/AppCard';
import { SkeletonBlock } from '@/src/components/SkeletonBlock';
import { SkeletonText } from '@/src/components/SkeletonText';
import { spacing } from '@/src/theme';

export function InventorySkeleton() {
  return (
    <View style={{ gap: spacing.lg }}>
      <SkeletonBlock height={48} />
      <View style={{ flexDirection: 'row', flexWrap: 'wrap', gap: spacing.sm }}>
        <SkeletonBlock height={34} width={90} />
        <SkeletonBlock height={34} width={100} />
        <SkeletonBlock height={34} width={120} />
      </View>
      {[0, 1, 2].map((item) => (
        <AppCard key={item}>
          <SkeletonText width="45%" />
          <SkeletonText width="22%" />
          <SkeletonText width="75%" />
          <SkeletonText width="60%" />
        </AppCard>
      ))}
    </View>
  );
}
