import {
  validateCreateProductForm,
  validateLoginForm,
  validatePositiveIntegerInput,
  validateTagUidInput,
} from '@/src/utils/forms';

describe('forms utils', () => {
  it('validates required login fields', () => {
    expect(validateLoginForm({ username: '', password: '' })).toEqual({
      username: 'required',
      password: 'required',
    });
  });

  it('validates required product name', () => {
    expect(
      validateCreateProductForm({
        sku: '',
        name: '',
        description: '',
      })
    ).toEqual({
      sku: 'required',
      name: 'required',
    });
  });

  it('validates product SKU as positive integer', () => {
    expect(
      validateCreateProductForm({
        sku: '0',
        name: 'Apples',
        description: '',
      })
    ).toEqual({
      sku: 'nonNegative',
    });
  });

  it('validates positive integer and UUID inputs', () => {
    expect(validatePositiveIntegerInput('3')).toBe(true);
    expect(validatePositiveIntegerInput('0')).toBe(false);
    expect(validateTagUidInput('123e4567-e89b-12d3-a456-426614174000')).toBe(true);
    expect(validateTagUidInput('TAG-1')).toBe(false);
  });
});
