import { StyleSheet, Text, View } from 'react-native';

import { AppButton } from '@/src/components/AppButton';
import { AppCard } from '@/src/components/AppCard';
import { AppScreen } from '@/src/components/AppScreen';
import { useAuth } from '@/src/providers/AuthProvider';
import { useI18n } from '@/src/providers/LocaleProvider';
import { colors, spacing } from '@/src/theme';

export function SettingsScreen() {
  const { logout, session } = useAuth();
  const { locale, setLocale, t } = useI18n();

  return (
    <AppScreen subtitle={t('settingsSubtitle')} title={t('settingsTitle')}>
      <AppCard>
        <Text style={styles.sectionTitle}>{t('languageSectionTitle')}</Text>
        <View style={styles.actionsRow}>
          <AppButton
            label={t('languageUkrainian')}
            onPress={() => setLocale('uk')}
            variant={locale === 'uk' ? 'primary' : 'secondary'}
          />
          <AppButton
            label={t('languageEnglish')}
            onPress={() => setLocale('en')}
            variant={locale === 'en' ? 'primary' : 'secondary'}
          />
        </View>
      </AppCard>

      <AppCard>
        <Text style={styles.sectionTitle}>{t('accountSectionTitle')}</Text>
        <View style={styles.infoRow}>
          <Text style={styles.label}>{t('accountIdLabel')}</Text>
          <Text style={styles.value}>{session?.id}</Text>
        </View>
        <View style={styles.infoRow}>
          <Text style={styles.label}>{t('accountEmailLabel')}</Text>
          <Text style={styles.value}>{session?.email}</Text>
        </View>
      </AppCard>

      <AppCard>
        <Text style={styles.sectionTitle}>{t('appModeTitle')}</Text>
        <Text style={styles.description}>{t('appModeDescription')}</Text>
        <AppButton label={t('signOut')} onPress={logout} variant="secondary" />
      </AppCard>
    </AppScreen>
  );
}

const styles = StyleSheet.create({
  sectionTitle: {
    color: colors.text,
    fontSize: 16,
    fontWeight: '700',
  },
  actionsRow: {
    flexDirection: 'row',
    gap: spacing.md,
  },
  infoRow: {
    gap: spacing.xs,
  },
  label: {
    color: colors.textMuted,
    fontSize: 13,
    fontWeight: '600',
  },
  value: {
    color: colors.text,
    fontSize: 16,
  },
  description: {
    color: colors.textMuted,
    fontSize: 14,
    lineHeight: 21,
  },
});
