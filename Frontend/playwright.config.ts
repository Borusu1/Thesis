import { defineConfig } from '@playwright/test';

const apiBaseUrl = process.env.E2E_API_BASE_URL ?? 'http://127.0.0.1:8000';

export default defineConfig({
  testDir: './e2e',
  timeout: 120_000,
  expect: {
    timeout: 10_000,
  },
  use: {
    baseURL: 'http://127.0.0.1:19006',
    trace: 'retain-on-failure',
    screenshot: 'only-on-failure',
  },
  webServer: {
    command: `EXPO_PUBLIC_API_BASE_URL=${apiBaseUrl} npm run web:ci`,
    url: 'http://127.0.0.1:19006',
    reuseExistingServer: !process.env.CI,
    timeout: 180_000,
  },
});
