import { fireEvent, render, waitFor } from '@testing-library/react-native';

import { LoginScreen } from '@/src/features/auth/screens/LoginScreen';
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

describe('LoginScreen', () => {
  it('shows validation for empty credentials', async () => {
    const login = jest.fn();
    mockedUseAuth.mockReturnValue({ login } as never);
    mockedUseI18n.mockReturnValue(createMockI18n() as never);

    const { getByText, findAllByText } = render(<LoginScreen />);

    fireEvent.press(getByText('Увійти'));

    expect(await findAllByText('Поле є обов’язковим.')).toHaveLength(2);
    expect(login).not.toHaveBeenCalled();
  });

  it('submits entered credentials', async () => {
    const login = jest.fn().mockResolvedValue(undefined);
    mockedUseAuth.mockReturnValue({ login } as never);
    mockedUseI18n.mockReturnValue(createMockI18n() as never);

    const { getByPlaceholderText, getByText } = render(<LoginScreen />);

    fireEvent.changeText(getByPlaceholderText('Введіть email'), 'user@example.com');
    fireEvent.changeText(getByPlaceholderText('Введіть пароль'), 'secret123');

    fireEvent.press(getByText('Увійти'));

    await waitFor(() => {
      expect(login).toHaveBeenCalledWith('user@example.com', 'secret123');
    });
  });
});
