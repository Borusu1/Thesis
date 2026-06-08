import { useState } from 'react';
import { StyleSheet, Text } from 'react-native';
import { router } from 'expo-router';

import { AppButton } from '@/src/components/AppButton';
import { AppCard } from '@/src/components/AppCard';
import { AppInput } from '@/src/components/AppInput';
import { AppScreen } from '@/src/components/AppScreen';
import { useI18n } from '@/src/providers/LocaleProvider';
import { useWarehouseService } from '@/src/providers/WarehouseServiceProvider';
import { colors } from '@/src/theme';
import { CreateProductFormErrors, CreateProductFormValues, validateCreateProductForm } from '@/src/utils/forms';

const initialValues: CreateProductFormValues = {
  sku: '',
  name: '',
  description: '',
};

export function AddProductScreen() {
  const { t } = useI18n();
  const warehouseService = useWarehouseService();
  const [values, setValues] = useState<CreateProductFormValues>(initialValues);
  const [errors, setErrors] = useState<CreateProductFormErrors>({});
  const [submitError, setSubmitError] = useState<string | null>(null);
  const [isSubmitting, setIsSubmitting] = useState(false);

  function setField<K extends keyof CreateProductFormValues>(field: K, value: CreateProductFormValues[K]) {
    setValues((current) => ({
      ...current,
      [field]: value,
    }));
  }

  async function handleSubmit() {
    const nextErrors = validateCreateProductForm(values);
    setErrors(nextErrors);
    setSubmitError(null);

    if (Object.keys(nextErrors).length) {
      return;
    }

    try {
      setIsSubmitting(true);
      const createdProduct = await warehouseService.createProduct({
        sku: Number(values.sku),
        name: values.name,
        description: values.description,
      });

      setValues(initialValues);
      router.push({
        pathname: '/(app)/product/[productId]',
        params: { productId: String(createdProduct.id) },
      });
    } catch (error) {
      setSubmitError(error instanceof Error ? error.message : t('genericError'));
    } finally {
      setIsSubmitting(false);
    }
  }

  return (
    <AppScreen subtitle={t('addProductSubtitle')} title={t('addProductTitle')}>
      <AppCard>
        <AppInput
          error={
            errors.sku
              ? errors.sku === 'nonNegative'
                ? t('validationPositiveInteger')
                : t('validationRequired')
              : undefined
          }
          keyboardType="number-pad"
          label={t('fieldSku')}
          onChangeText={(value) => setField('sku', value)}
          placeholder={t('fieldSkuPlaceholder')}
          value={values.sku}
        />
        <AppInput
          error={errors.name ? t('validationRequired') : undefined}
          label={t('fieldName')}
          onChangeText={(value) => setField('name', value)}
          placeholder={t('fieldNamePlaceholder')}
          value={values.name}
        />
        <AppInput
          label={t('fieldDescription')}
          multiline
          numberOfLines={4}
          onChangeText={(value) => setField('description', value)}
          placeholder={t('fieldDescriptionPlaceholder')}
          value={values.description}
        />
        {submitError ? <Text style={styles.submitError}>{submitError}</Text> : null}
        <AppButton
          disabled={isSubmitting}
          label={isSubmitting ? t('saving') : t('createProductAction')}
          onPress={handleSubmit}
        />
      </AppCard>
    </AppScreen>
  );
}

const styles = StyleSheet.create({
  submitError: {
    color: colors.danger,
    fontSize: 14,
    lineHeight: 20,
  },
});
