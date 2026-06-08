import { StyleProp, ViewStyle } from 'react-native';

import { SkeletonBlock } from '@/src/components/SkeletonBlock';

type SkeletonTextProps = {
  width?: number | `${number}%`;
  style?: StyleProp<ViewStyle>;
};

export function SkeletonText({ width = '100%', style }: SkeletonTextProps) {
  return <SkeletonBlock height={14} style={style} width={width} />;
}
