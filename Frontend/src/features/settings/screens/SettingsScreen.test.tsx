import { fireEvent, render } from '@testing-library/react-native';

import { SettingsScreen } from '@/src/features/settings/screens/SettingsScreen';
import { useAuth } from '@/src/providers/AuthProvider';
import { useI18n } from '@/src/providers/LocaleProvider';
import { createMockI18n } from '@/test-utils/mockI18n';

jest.mock('@/src/providers/AuthProvider', () => ({
  useAuth: jest.fn(),
}));

jest.mock('@/src/providers/LocaleProvider', () => ({
  useI18n: jest.fn(),
}));

const mockedUseAuth = jest.mocked(useAuth);
const mockedUseI18n = jest.mocked(useI18n);

describe('SettingsScreen', () => {
  it('switches language and signs out', () => {
    const logout = jest.fn();
    const i18n = createMockI18n();

    mockedUseAuth.mockReturnValue({
      logout,
      session: {
        id: 1,
        email: 'demo@example.com',
      },
    } as never);
    mockedUseI18n.mockReturnValue(i18n as never);

    const { getByText } = render(<SettingsScreen />);

    fireEvent.press(getByText('English'));
    fireEvent.press(getByText('Вийти'));

    expect(i18n.setLocale).toHaveBeenCalledWith('en');
    expect(logout).toHaveBeenCalled();
  });
});
