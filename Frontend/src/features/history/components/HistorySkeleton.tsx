import { View } from 'react-native';

import { AppCard } from '@/src/components/AppCard';
import { SkeletonText } from '@/src/components/SkeletonText';
import { spacing } from '@/src/theme';

export function HistorySkeleton() {
  return (
    <View style={{ gap: spacing.lg }}>
      {[0, 1, 2].map((item) => (
        <AppCard key={item}>
          <SkeletonText width="55%" />
          <SkeletonText width="35%" />
          <SkeletonText width="85%" />
          <SkeletonText width="45%" />
        </AppCard>
      ))}
    </View>
  );
}
