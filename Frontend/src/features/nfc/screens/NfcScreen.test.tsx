import { fireEvent, render, waitFor } from '@testing-library/react-native';
import { Platform } from 'react-native';
import { router } from 'expo-router';

import { NfcScreen } from '@/src/features/nfc/screens/NfcScreen';
import { useI18n } from '@/src/providers/LocaleProvider';
import { useWarehouseService } from '@/src/providers/WarehouseServiceProvider';
import { scanNfcTagId } from '@/src/services/nfc/nfcScanner';
import { createMockI18n } from '@/test-utils/mockI18n';

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

jest.mock('@/src/services/nfc/nfcScanner', () => ({
  scanNfcTagId: jest.fn(),
}));

const mockedUseI18n = jest.mocked(useI18n);
const mockedUseWarehouseService = jest.mocked(useWarehouseService);
const mockedScanNfcTagId = jest.mocked(scanNfcTagId);

describe('NfcScreen', () => {
  const originalPlatform = Platform.OS;

  afterEach(() => {
    Object.defineProperty(Platform, 'OS', {
      configurable: true,
      value: originalPlatform,
    });
  });

  it('supports manual lookup on web', async () => {
    Object.defineProperty(Platform, 'OS', {
      configurable: true,
      value: 'web',
    });

    mockedUseI18n.mockReturnValue(createMockI18n() as never);
    mockedUseWarehouseService.mockReturnValue({
      lookupTag: jest.fn().mockResolvedValue({
        tagUid: '123e4567-e89b-12d3-a456-426614174000',
        activeUsage: {
          id: 10,
          tagUid: '123e4567-e89b-12d3-a456-426614174000',
          productId: 1,
          productNameSnapshot: 'Яблука',
          quantityInitial: 18,
          quantityCurrent: 12,
          arrivedAt: '2026-03-22T09:00:00.000Z',
          warehouseLocation: 'A-01',
          closedAt: null,
          events: [],
        },
        usages: [],
      }),
    } as never);

    const { getByPlaceholderText, getByText } = render(<NfcScreen />);

    fireEvent.changeText(
      getByPlaceholderText('123e4567-e89b-12d3-a456-426614174000'),
      '123e4567-e89b-12d3-a456-426614174000'
    );
    fireEvent.press(getByText('Знайти мітку'));

    await waitFor(() => {
      expect(getByText('Активний товар знайдено')).toBeTruthy();
    });
  });

  it('scans a native tag and opens the linked product', async () => {
    Object.defineProperty(Platform, 'OS', {
      configurable: true,
      value: 'ios',
    });

    mockedUseI18n.mockReturnValue(createMockI18n() as never);
    mockedScanNfcTagId.mockResolvedValue('123e4567-e89b-12d3-a456-426614174000');
    mockedUseWarehouseService.mockReturnValue({
      lookupTag: jest.fn().mockResolvedValue({
        tagUid: '123e4567-e89b-12d3-a456-426614174000',
        activeUsage: {
          id: 10,
          tagUid: '123e4567-e89b-12d3-a456-426614174000',
          productId: 1,
          productNameSnapshot: 'Яблука',
          quantityInitial: 18,
          quantityCurrent: 12,
          arrivedAt: '2026-03-22T09:00:00.000Z',
          warehouseLocation: 'A-01',
          closedAt: null,
          events: [],
        },
        usages: [],
      }),
    } as never);

    const { getByText } = render(<NfcScreen />);

    fireEvent.press(getByText('Сканувати мітку'));

    await waitFor(() => {
      expect(getByText('Активний товар знайдено')).toBeTruthy();
    });

    fireEvent.press(getByText('Відкрити картку товару'));
    expect(router.push).toHaveBeenCalled();
  });

  it('shows an error when the tag is unknown', async () => {
    Object.defineProperty(Platform, 'OS', {
      configurable: true,
      value: 'web',
    });

    mockedUseI18n.mockReturnValue(createMockI18n() as never);
    mockedUseWarehouseService.mockReturnValue({
      lookupTag: jest.fn().mockResolvedValue(null),
    } as never);

    const { getByPlaceholderText, getByText, findByText } = render(<NfcScreen />);

    fireEvent.changeText(
      getByPlaceholderText('123e4567-e89b-12d3-a456-426614174000'),
      '123e4567-e89b-12d3-a456-426614174000'
    );
    fireEvent.press(getByText('Знайти мітку'));

    expect(await findByText('Мітка не знайдена у backend.')).toBeTruthy();
  });
});
