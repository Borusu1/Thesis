import { Link } from 'expo-router';
import { SafeAreaView, StyleSheet, Text, View } from 'react-native';

import { useI18n } from '@/src/providers/LocaleProvider';
import { colors, spacing } from '@/src/theme';

export default function NotFoundScreen() {
  const { t } = useI18n();

  return (
    <SafeAreaView style={styles.safeArea}>
      <View style={styles.container}>
        <Text style={styles.title}>{t('notFoundTitle')}</Text>
        <Text style={styles.description}>{t('notFoundDescription')}</Text>
        <Link href="/" style={styles.link}>
          {t('backToHome')}
        </Link>
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
    flex: 1,
    justifyContent: 'center',
    padding: spacing.lg,
    gap: spacing.sm,
  },
  title: {
    color: colors.text,
    fontSize: 28,
    fontWeight: '700',
  },
  description: {
    color: colors.textMuted,
    fontSize: 16,
    lineHeight: 24,
  },
  link: {
    color: colors.primary,
    fontSize: 16,
    fontWeight: '600',
  },
});
