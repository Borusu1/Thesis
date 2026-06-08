import { View } from 'react-native';

import { AppCard } from '@/src/components/AppCard';
import { SkeletonBlock } from '@/src/components/SkeletonBlock';
import { SkeletonText } from '@/src/components/SkeletonText';
import { spacing } from '@/src/theme';

export function ProductDetailsSkeleton() {
  return (
    <View style={{ gap: spacing.lg }}>
      <AppCard>
        <SkeletonBlock height={24} width="35%" />
        <SkeletonText width="55%" />
        <SkeletonText width="40%" />
        <SkeletonText width="65%" />
      </AppCard>
      <AppCard>
        <SkeletonText width="38%" />
        <SkeletonText width="82%" />
        <SkeletonText width="72%" />
      </AppCard>
      <AppCard>
        <SkeletonText width="42%" />
        <SkeletonBlock height={40} />
        <SkeletonBlock height={88} />
      </AppCard>
    </View>
  );
}
