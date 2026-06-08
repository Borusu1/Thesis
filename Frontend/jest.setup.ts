import mockAsyncStorage from '@react-native-async-storage/async-storage/jest/async-storage-mock';

jest.mock('@react-native-async-storage/async-storage', () => mockAsyncStorage);
jest.mock('expo-localization', () => ({
  getLocales: () => [{ languageCode: 'uk' }],
}));

beforeEach(async () => {
  await mockAsyncStorage.clear();
  jest.clearAllMocks();
});

const originalWarn = console.warn;

beforeAll(() => {
  jest.spyOn(console, 'warn').mockImplementation((message?: unknown, ...rest: unknown[]) => {
    if (typeof message === 'string' && message.includes('SafeAreaView has been deprecated')) {
      return;
    }

    originalWarn(message, ...rest);
  });
});
