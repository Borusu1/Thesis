import { ActivityIndicator, SafeAreaView, StyleSheet, Text, View } from 'react-native';

import { useI18n } from '@/src/providers/LocaleProvider';
import { colors, spacing } from '@/src/theme';

export function AppLoadingScreen() {
  const { t } = useI18n();

  return (
    <SafeAreaView style={styles.safeArea}>
      <View style={styles.container}>
        <ActivityIndicator color={colors.primary} size="large" />
        <Text style={styles.label}>{t('loading')}</Text>
      </View>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  safeArea: {
    flex: 1,
    backgroundColor: colors.background,
  },
  container: {
    alignItems: 'center',
    flex: 1,
    gap: spacing.md,
    justifyContent: 'center',
  },
  label: {
    color: colors.textMuted,
    fontSize: 14,
    fontWeight: '500',
  },
});
