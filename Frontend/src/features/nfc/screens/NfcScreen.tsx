import { useState } from 'react';
import { Platform, StyleSheet, Text, View } from 'react-native';
import { router } from 'expo-router';

import { AppButton } from '@/src/components/AppButton';
import { AppCard } from '@/src/components/AppCard';
import { AppInput } from '@/src/components/AppInput';
import { AppScreen } from '@/src/components/AppScreen';
import { EmptyState } from '@/src/components/EmptyState';
import { StatusBadge } from '@/src/components/StatusBadge';
import { useI18n } from '@/src/providers/LocaleProvider';
import { useWarehouseService } from '@/src/providers/WarehouseServiceProvider';
import { scanNfcTagId } from '@/src/services/nfc/nfcScanner';
import { colors, spacing } from '@/src/theme';
import { TagLookupResult } from '@/src/types/warehouse';
import { formatQuantity } from '@/src/utils/format';
import { extractTagUid } from '@/src/utils/tag';

export function NfcScreen() {
  const { t } = useI18n();
  const warehouseService = useWarehouseService();
  const [isScanning, setIsScanning] = useState(false);
  const [isLookingUp, setIsLookingUp] = useState(false);
  const [scanError, setScanError] = useState<string | null>(null);
  const [lastTagUid, setLastTagUid] = useState<string | null>(null);
  const [manualTagUid, setManualTagUid] = useState('');
  const [lookupResult, setLookupResult] = useState<TagLookupResult | null>(null);

  async function runLookup(rawTagValue: string) {
    const normalizedTagUid = extractTagUid(rawTagValue);

    if (!normalizedTagUid) {
      setLookupResult(null);
      setScanError(t('validationInvalidUuid'));
      return;
    }

    try {
      setIsLookingUp(true);
      setScanError(null);
      const result = await warehouseService.lookupTag(normalizedTagUid);

      setLastTagUid(normalizedTagUid);
      setLookupResult(result);
      setManualTagUid(normalizedTagUid);

      if (!result) {
        setScanError(t('nfcUnknownTag'));
      }
    } catch (error) {
      setLookupResult(null);
      setScanError(error instanceof Error ? error.message : t('genericError'));
    } finally {
      setIsLookingUp(false);
    }
  }

  async function handleScan() {
    try {
      setIsScanning(true);
      setScanError(null);
      const tagUid = await scanNfcTagId();
      await runLookup(tagUid);
    } catch (error) {
      const code = error instanceof Error ? error.message : 'UNKNOWN';

      if (code === 'NFC_DISABLED') {
        setScanError(t('nfcDisabled'));
      } else if (code === 'NFC_NOT_SUPPORTED') {
        setScanError(t('nfcNotSupported'));
      } else if (code === 'NFC_TAG_ID_NOT_FOUND') {
        setScanError(t('nfcTagIdMissing'));
      } else {
        setScanError(t('genericError'));
      }
    } finally {
      setIsScanning(false);
    }
  }

  return (
    <AppScreen subtitle={t('nfcSubtitle')} title={t('nfcTitle')}>
      {Platform.OS === 'web' ? (
        <AppCard>
          <Text style={styles.sectionTitle}>{t('nfcManualLookupTitle')}</Text>
          <Text style={styles.description}>{t('nfcManualLookupDescription')}</Text>
        </AppCard>
      ) : (
        <AppCard>
          <Text style={styles.sectionTitle}>{t('nfcReadyTitle')}</Text>
          <Text style={styles.description}>{t('nfcReadyDescription')}</Text>
          <AppButton
            disabled={isScanning}
            label={isScanning ? t('nfcScanningAction') : t('nfcScanAction')}
            onPress={handleScan}
          />
        </AppCard>
      )}

      <AppCard>
        <AppInput
          error={scanError === t('validationInvalidUuid') ? scanError : undefined}
          label={t('fieldTagUid')}
          onChangeText={setManualTagUid}
          placeholder={t('fieldTagUidPlaceholder')}
          value={manualTagUid}
        />
        <AppButton
          disabled={isLookingUp}
          label={isLookingUp ? t('nfcLookupLoading') : t('nfcLookupAction')}
          onPress={() => runLookup(manualTagUid)}
        />
      </AppCard>

      {scanError && scanError !== t('validationInvalidUuid') ? (
        <AppCard>
          <Text style={styles.errorText}>{scanError}</Text>
          {lastTagUid ? <Text style={styles.metaText}>{t('nfcLastTag')}: {lastTagUid}</Text> : null}
        </AppCard>
      ) : null}

      {lookupResult ? (
        lookupResult.activeUsage ? (
          <AppCard>
            <Text style={styles.sectionTitle}>{t('nfcFoundProductTitle')}</Text>
            <View style={styles.productHeader}>
              <View style={styles.productHeaderText}>
                <Text style={styles.productName}>{lookupResult.activeUsage.productNameSnapshot}</Text>
                <Text style={styles.metaText}>{lookupResult.activeUsage.tagUid}</Text>
              </View>
              <StatusBadge status={lookupResult.activeUsage.quantityCurrent > 0 ? 'inStock' : 'outOfStock'} />
            </View>
            <Text style={styles.metaText}>
              {formatQuantity(lookupResult.activeUsage.quantityCurrent)}
            </Text>
            {lookupResult.activeUsage.warehouseLocation ? (
              <Text style={styles.metaText}>
                {t('inventoryLocationLabel')}: {lookupResult.activeUsage.warehouseLocation}
              </Text>
            ) : null}
            <AppButton
              label={t('nfcOpenProduct')}
              onPress={() =>
                router.push({
                  pathname: '/(app)/product/[productId]',
                  params: { productId: String(lookupResult.activeUsage?.productId) },
                })
              }
            />
          </AppCard>
        ) : (
          <AppCard>
            <Text style={styles.sectionTitle}>{t('nfcFreeTagTitle')}</Text>
            <Text style={styles.metaText}>{lookupResult.tagUid}</Text>
            <Text style={styles.metaText}>{t('nfcFreeTagDescription')}</Text>
          </AppCard>
        )
      ) : (
        <EmptyState description={t('nfcEmptyDescription')} title={t('nfcEmptyTitle')} />
      )}
    </AppScreen>
  );
}

const styles = StyleSheet.create({
  sectionTitle: {
    color: colors.text,
    fontSize: 17,
    fontWeight: '700',
  },
  description: {
    color: colors.textMuted,
    fontSize: 14,
    lineHeight: 21,
  },
  errorText: {
    color: colors.danger,
    fontSize: 15,
    fontWeight: '600',
    lineHeight: 22,
  },
  metaText: {
    color: colors.textMuted,
    fontSize: 14,
    lineHeight: 20,
  },
  productHeader: {
    alignItems: 'flex-start',
    flexDirection: 'row',
    gap: spacing.md,
    justifyContent: 'space-between',
  },
  productHeaderText: {
    flex: 1,
    gap: spacing.xs,
  },
  productName: {
    color: colors.text,
    fontSize: 16,
    fontWeight: '700',
  },
});
