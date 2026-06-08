import { fireEvent, render, waitFor } from '@testing-library/react-native';

import { AddProductScreen } from '@/src/features/inventory/screens/AddProductScreen';
import { useI18n } from '@/src/providers/LocaleProvider';
import { useWarehouseService } from '@/src/providers/WarehouseServiceProvider';
import { createMockI18n } from '@/test-utils/mockI18n';

const mockPush = jest.fn();

jest.mock('expo-router', () => ({
  router: {
    push: (...args: unknown[]) => mockPush(...args),
  },
}));

jest.mock('@/src/providers/LocaleProvider', () => ({
  useI18n: jest.fn(),
}));

jest.mock('@/src/providers/WarehouseServiceProvider', () => ({
  useWarehouseService: jest.fn(),
}));

const mockedUseI18n = jest.mocked(useI18n);
const mockedUseWarehouseService = jest.mocked(useWarehouseService);

describe('AddProductScreen', () => {
  it('validates required fields', async () => {
    mockedUseI18n.mockReturnValue(createMockI18n() as never);
    mockedUseWarehouseService.mockReturnValue({
      createProduct: jest.fn(),
    } as never);

    const { getByText, findAllByText } = render(<AddProductScreen />);

    fireEvent.press(getByText('Створити товар'));

    expect((await findAllByText('Поле є обов’язковим.')).length).toBe(2);
  });

  it('creates a product and opens the product card', async () => {
    const createProduct = jest.fn().mockResolvedValue({ id: 11 });
    mockedUseI18n.mockReturnValue(createMockI18n() as never);
    mockedUseWarehouseService.mockReturnValue({
      createProduct,
    } as never);

    const { getByPlaceholderText, getByText } = render(<AddProductScreen />);

    fireEvent.changeText(getByPlaceholderText('Наприклад, 1001'), '1001');
    fireEvent.changeText(getByPlaceholderText('Наприклад, Температурний сенсор'), 'Новий товар');
    fireEvent.changeText(getByPlaceholderText('Короткий опис товару'), 'Опис нового товару');

    fireEvent.press(getByText('Створити товар'));

    await waitFor(() => {
      expect(createProduct).toHaveBeenCalledWith({
        sku: 1001,
        name: 'Новий товар',
        description: 'Опис нового товару',
      });
      expect(mockPush).toHaveBeenCalled();
    });
  });

  it('shows a backend error message', async () => {
    const createProduct = jest.fn().mockRejectedValue(new Error('Product already exists'));
    mockedUseI18n.mockReturnValue(createMockI18n() as never);
    mockedUseWarehouseService.mockReturnValue({
      createProduct,
    } as never);

    const { getByPlaceholderText, getByText, findByText } = render(<AddProductScreen />);

    fireEvent.changeText(getByPlaceholderText('Наприклад, 1001'), '1001');
    fireEvent.changeText(getByPlaceholderText('Наприклад, Температурний сенсор'), 'Новий товар');
    fireEvent.press(getByText('Створити товар'));

    expect(await findByText('Product already exists')).toBeTruthy();
  });

  it('validates positive SKU', async () => {
    mockedUseI18n.mockReturnValue(createMockI18n() as never);
    mockedUseWarehouseService.mockReturnValue({
      createProduct: jest.fn(),
    } as never);

    const { getByPlaceholderText, getByText, findByText } = render(<AddProductScreen />);

    fireEvent.changeText(getByPlaceholderText('Наприклад, 1001'), '0');
    fireEvent.changeText(getByPlaceholderText('Наприклад, Температурний сенсор'), 'Новий товар');
    fireEvent.press(getByText('Створити товар'));

    expect(await findByText('Введіть ціле число, більше за нуль.')).toBeTruthy();
  });
});
