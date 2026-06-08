import { StyleSheet, Text, View } from 'react-native';

import { AppCard } from '@/src/components/AppCard';
import { AppScreen } from '@/src/components/AppScreen';
import { useI18n } from '@/src/providers/LocaleProvider';
import { colors, radius, spacing } from '@/src/theme';

type ScreenKey = 'dashboard' | 'inventory' | 'addProduct' | 'history' | 'nfc';

const translationMap: Record<
  ScreenKey,
  {
    titleKey:
      | 'dashboardTitle'
      | 'inventoryTitle'
      | 'addProductTitle'
      | 'historyTitle'
      | 'nfcTitle';
    subtitleKey:
      | 'dashboardSubtitle'
      | 'inventorySubtitle'
      | 'addProductSubtitle'
      | 'historySubtitle'
      | 'nfcSubtitle';
  }
> = {
  dashboard: { titleKey: 'dashboardTitle', subtitleKey: 'dashboardSubtitle' },
  inventory: { titleKey: 'inventoryTitle', subtitleKey: 'inventorySubtitle' },
  addProduct: { titleKey: 'addProductTitle', subtitleKey: 'addProductSubtitle' },
  history: { titleKey: 'historyTitle', subtitleKey: 'historySubtitle' },
  nfc: { titleKey: 'nfcTitle', subtitleKey: 'nfcSubtitle' },
};

type PlaceholderScreenProps = {
  screenKey: ScreenKey;
};

export function PlaceholderScreen({ screenKey }: PlaceholderScreenProps) {
  const { t } = useI18n();
  const keys = translationMap[screenKey];

  return (
    <AppScreen subtitle={t(keys.subtitleKey)} title={t(keys.titleKey)}>
      <AppCard>
        <View style={styles.badge}>
          <Text style={styles.badgeText}>{t('comingSoonBadge')}</Text>
        </View>
        <Text style={styles.text}>{t('placeholderBody')}</Text>
      </AppCard>
    </AppScreen>
  );
}

const styles = StyleSheet.create({
  badge: {
    alignSelf: 'flex-start',
    backgroundColor: colors.primarySoft,
    borderRadius: radius.pill,
    paddingHorizontal: spacing.md,
    paddingVertical: spacing.sm,
  },
  badgeText: {
    color: colors.primary,
    fontSize: 12,
    fontWeight: '700',
    letterSpacing: 0.5,
    textTransform: 'uppercase',
  },
  text: {
    color: colors.text,
    fontSize: 15,
    lineHeight: 22,
  },
});
