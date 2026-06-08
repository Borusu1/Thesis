import { render, waitFor } from '@testing-library/react-native';

import { HistoryScreen } from '@/src/features/history/screens/HistoryScreen';
import { useI18n } from '@/src/providers/LocaleProvider';
import { useWarehouseService } from '@/src/providers/WarehouseServiceProvider';
import { createMockI18n } from '@/test-utils/mockI18n';

jest.mock('@react-navigation/native', () => ({
  useIsFocused: () => true,
}));

jest.mock('expo-router', () => ({
  router: {
    push: jest.fn(),
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

describe('HistoryScreen', () => {
  it('shows inventory events from the service', async () => {
    mockedUseI18n.mockReturnValue(createMockI18n() as never);
    mockedUseWarehouseService.mockReturnValue({
      getOperations: jest.fn().mockResolvedValue([
        {
          id: 1,
          usageId: 22,
          productId: 1,
          productNameSnapshot: 'Температурний сенсор',
          type: 'shipment_full',
          quantity: 5,
          quantityDelta: -5,
          note: 'Повне відвантаження',
          actor: 'API',
          createdAt: '2026-03-22T10:15:00.000Z',
          tagUid: '123e4567-e89b-12d3-a456-426614174000',
        },
      ]),
    } as never);

    const { getAllByText, getByText } = render(<HistoryScreen />);

    await waitFor(() => {
      expect(getByText('Температурний сенсор')).toBeTruthy();
      expect(getAllByText('Повне відвантаження').length).toBeGreaterThan(0);
    });
  });
});
