import { renderRouter, waitFor } from 'expo-router/testing-library';
import React from 'react';

import IndexRoute from '@/app/index';
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

describe('root index route', () => {
  it('redirects guests to the login route', async () => {
    mockedUseAuth.mockReturnValue({ isReady: true, session: null } as never);
    mockedUseI18n.mockReturnValue(createMockI18n() as never);

    const result = renderRouter(
      {
        index: IndexRoute,
        '(auth)/login': () => null,
        '(app)/(tabs)/dashboard': () => null,
      },
      { initialUrl: '/' }
    );

    await waitFor(() => {
      expect(result.getPathname()).toBe('/login');
    });
  });

  it('redirects authenticated users to dashboard', async () => {
    mockedUseAuth.mockReturnValue({
      isReady: true,
      session: { id: 1, email: 'demo@example.com', isActive: true, accessToken: 'token' },
    } as never);
    mockedUseI18n.mockReturnValue(createMockI18n() as never);

    const result = renderRouter(
      {
        index: IndexRoute,
        '(auth)/login': () => null,
        '(app)/(tabs)/dashboard': () => null,
      },
      { initialUrl: '/' }
    );

    await waitFor(() => {
      expect(result.getPathname()).toBe('/dashboard');
    });
  });
});
