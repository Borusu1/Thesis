import { render, waitFor } from '@testing-library/react-native';

import { DashboardScreen } from '@/src/features/dashboard/screens/DashboardScreen';
import { useI18n } from '@/src/providers/LocaleProvider';
import { useWarehouseService } from '@/src/providers/WarehouseServiceProvider';
import { createMockI18n } from '@/test-utils/mockI18n';

jest.mock('@react-navigation/native', () => ({
  useIsFocused: () => true,
}));

jest.mock('@/src/providers/LocaleProvider', () => ({
  useI18n: jest.fn(),
}));

jest.mock('@/src/providers/WarehouseServiceProvider', () => ({
  useWarehouseService: jest.fn(),
}));

const mockedUseI18n = jest.mocked(useI18n);
const mockedUseWarehouseService = jest.mocked(useWarehouseService);

describe('DashboardScreen', () => {
  it('renders metrics and recent activity from API data', async () => {
    mockedUseI18n.mockReturnValue(createMockI18n() as never);
    mockedUseWarehouseService.mockReturnValue({
      getDashboardSummary: jest.fn().mockResolvedValue({
        totalProducts: 2,
        totalUnits: 15,
        lowStockCount: 0,
        outOfStockCount: 1,
        recentOperations: [
          {
            id: 1,
            usageId: 10,
            productId: 2,
            productNameSnapshot: 'Абрикоси',
            type: 'receipt',
            quantity: 7,
            quantityDelta: 7,
            note: 'Нове надходження',
            actor: 'API',
            createdAt: '2026-03-24T09:00:00.000Z',
            tagUid: '123e4567-e89b-12d3-a456-426614174000',
          },
        ],
      }),
      getProducts: jest.fn().mockResolvedValue([
        {
          id: 1,
          name: 'Яблука',
          description: 'Палета яблук',
          quantityOnHand: 0,
          createdAt: '2026-03-24T08:00:00.000Z',
          status: 'outOfStock',
        },
      ]),
    } as never);

    const { getByText } = render(<DashboardScreen />);

    await waitFor(() => {
      expect(getByText('Товарів')).toBeTruthy();
      expect(getByText('Нове надходження')).toBeTruthy();
    });
  });
});
