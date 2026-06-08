import { useEffect, useRef } from 'react';
import { Animated, StyleProp, StyleSheet, ViewStyle } from 'react-native';

import { colors, radius } from '@/src/theme';

type SkeletonBlockProps = {
  height: number;
  width?: number | `${number}%`;
  style?: StyleProp<ViewStyle>;
  borderRadius?: number;
};

export function SkeletonBlock({
  height,
  width = '100%',
  style,
  borderRadius = radius.md,
}: SkeletonBlockProps) {
  const animatedValue = useRef(new Animated.Value(0.45)).current;

  useEffect(() => {
    const animation = Animated.loop(
      Animated.sequence([
        Animated.timing(animatedValue, {
          toValue: 1,
          duration: 900,
          useNativeDriver: true,
        }),
        Animated.timing(animatedValue, {
          toValue: 0.45,
          duration: 900,
          useNativeDriver: true,
        }),
      ])
    );

    animation.start();

    return () => {
      animation.stop();
    };
  }, [animatedValue]);

  return (
    <Animated.View
      style={[
        styles.block,
        {
          borderRadius,
          height,
          opacity: animatedValue,
          width,
        },
        style,
      ]}
    />
  );
}

const styles = StyleSheet.create({
  block: {
    backgroundColor: colors.placeholder,
  },
});
