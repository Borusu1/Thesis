import { fireEvent, render, waitFor } from '@testing-library/react-native';

import { ProductDetailsScreen } from '@/src/features/inventory/screens/ProductDetailsScreen';
import { useI18n } from '@/src/providers/LocaleProvider';
import { useWarehouseService } from '@/src/providers/WarehouseServiceProvider';
import { createMockI18n } from '@/test-utils/mockI18n';

jest.mock('@react-navigation/native', () => ({
  useIsFocused: () => true,
}));

jest.mock('expo-router', () => ({
  useLocalSearchParams: () => ({ productId: '1' }),
}));

jest.mock('@/src/providers/LocaleProvider', () => ({
  useI18n: jest.fn(),
}));

jest.mock('@/src/providers/WarehouseServiceProvider', () => ({
  useWarehouseService: jest.fn(),
}));

const mockedUseI18n = jest.mocked(useI18n);
const mockedUseWarehouseService = jest.mocked(useWarehouseService);

describe('ProductDetailsScreen', () => {
  it('loads product details and submits shipment actions', async () => {
    const getProductById = jest
      .fn()
      .mockResolvedValue({
        id: 1,
        name: 'Яблука',
        description: 'Палета яблук',
        quantityOnHand: 18,
        createdAt: '2026-03-22T09:00:00.000Z',
        status: 'inStock',
      });
    const getOperations = jest
      .fn()
      .mockResolvedValue([
        {
          id: 1,
          usageId: 10,
          productId: 1,
          productNameSnapshot: 'Яблука',
          type: 'receipt',
          quantity: 18,
          quantityDelta: 18,
          note: 'Надходження',
          actor: 'API',
          createdAt: '2026-03-22T09:00:00.000Z',
          tagUid: '123e4567-e89b-12d3-a456-426614174000',
        },
      ]);
    const getActiveTags = jest
      .fn()
      .mockResolvedValue([
        {
          id: 10,
          tagUid: '123e4567-e89b-12d3-a456-426614174000',
          productId: 1,
          productNameSnapshot: 'Яблука',
          quantityInitial: 18,
          quantityCurrent: 18,
          arrivedAt: '2026-03-22T09:00:00.000Z',
          warehouseLocation: 'A-01',
        },
      ]);
    const createPartialShipment = jest.fn().mockResolvedValue(undefined);
    const createFullShipment = jest.fn().mockResolvedValue(undefined);

    mockedUseI18n.mockReturnValue(createMockI18n() as never);
    mockedUseWarehouseService.mockReturnValue({
      getProductById,
      getOperations,
      getActiveTags,
      createPartialShipment,
      createFullShipment,
    } as never);

    const { getAllByText, getByText } = render(<ProductDetailsScreen />);

    await waitFor(() => {
      expect(getAllByText('Яблука').length).toBeGreaterThan(0);
    });

    fireEvent.press(getByText('Часткове відвантаження'));
    await waitFor(() => {
      expect(createPartialShipment).toHaveBeenCalledWith(
        expect.objectContaining({
          tagUid: '123e4567-e89b-12d3-a456-426614174000',
          quantity: 1,
        })
      );
    });

    fireEvent.press(getByText('Повне відвантаження'));
    await waitFor(() => {
      expect(createFullShipment).toHaveBeenCalledWith(
        expect.objectContaining({
          tagUid: '123e4567-e89b-12d3-a456-426614174000',
        })
      );
    });
  });
});
