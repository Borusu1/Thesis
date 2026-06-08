import { extractTagUid } from '@/src/utils/tag';

export type ValidationErrorCode = 'required' | 'nonNegative' | 'invalidUuid';

export type LoginFormValues = {
  username: string;
  password: string;
};

export type LoginFormErrors = Partial<Record<keyof LoginFormValues, ValidationErrorCode>>;

export type CreateProductFormValues = {
  sku: string;
  name: string;
  description: string;
};

export type CreateProductFormErrors = Partial<Record<'sku' | 'name', ValidationErrorCode>>;

export function validateLoginForm(values: LoginFormValues): LoginFormErrors {
  const errors: LoginFormErrors = {};

  if (!values.username.trim()) {
    errors.username = 'required';
  }

  if (!values.password.trim()) {
    errors.password = 'required';
  }

  return errors;
}

export function validateCreateProductForm(values: CreateProductFormValues): CreateProductFormErrors {
  const errors: CreateProductFormErrors = {};

  if (!validatePositiveIntegerInput(values.sku)) {
    errors.sku = values.sku.trim() ? 'nonNegative' : 'required';
  }

  if (!values.name.trim()) {
    errors.name = 'required';
  }

  return errors;
}

export function validatePositiveIntegerInput(value: string) {
  const numericValue = Number(value);
  return value.trim() !== '' && Number.isInteger(numericValue) && numericValue > 0;
}

export function validateTagUidInput(value: string) {
  return extractTagUid(value) !== null;
}
