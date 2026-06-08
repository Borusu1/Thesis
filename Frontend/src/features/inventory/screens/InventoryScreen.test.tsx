import { fireEvent, render, waitFor } from '@testing-library/react-native';

import { InventoryScreen } from '@/src/features/inventory/screens/InventoryScreen';
import { useI18n } from '@/src/providers/LocaleProvider';
import { useWarehouseService } from '@/src/providers/WarehouseServiceProvider';
import { createMockI18n } from '@/test-utils/mockI18n';

const mockPush = jest.fn();

jest.mock('@react-navigation/native', () => ({
  useIsFocused: () => true,
}));

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

describe('InventoryScreen', () => {
  it('filters products by description and opens a card', async () => {
    mockedUseI18n.mockReturnValue(createMockI18n() as never);
    mockedUseWarehouseService.mockReturnValue({
      getProducts: jest.fn().mockResolvedValue([
        {
          id: 1,
          name: 'Яблука',
          description: 'Соковиті яблука',
          quantityOnHand: 18,
          createdAt: '2026-03-22T09:00:00.000Z',
          status: 'inStock',
        },
        {
          id: 2,
          name: 'Абрикоси',
          description: 'Літня партія',
          quantityOnHand: 0,
          createdAt: '2026-03-22T10:15:00.000Z',
          status: 'outOfStock',
        },
      ]),
    } as never);

    const { getByPlaceholderText, getByText, queryByText } = render(<InventoryScreen />);

    await waitFor(() => {
      expect(getByText('Яблука')).toBeTruthy();
    });

    fireEvent.changeText(getByPlaceholderText('Пошук за назвою, описом або ID товару'), 'літня');

    await waitFor(() => {
      expect(getByText('Абрикоси')).toBeTruthy();
      expect(queryByText('Яблука')).toBeNull();
    });

    fireEvent.press(getByText('Абрикоси'));
    expect(mockPush).toHaveBeenCalled();
  });
});
