import { translations } from '@/src/i18n/translations';
import { LocaleCode } from '@/src/types/app';

function resolveNestedValue(source: Record<string, unknown>, key: string) {
  return key.split('.').reduce<unknown>((accumulator, part) => {
    if (accumulator && typeof accumulator === 'object' && part in accumulator) {
      return (accumulator as Record<string, unknown>)[part];
    }

    return undefined;
  }, source);
}

export function createMockI18n(locale: LocaleCode = 'uk') {
  return {
    locale,
    isReady: true,
    setLocale: jest.fn(),
    t: (key: string) => {
      const value = resolveNestedValue(translations[locale] as Record<string, unknown>, key);

      return typeof value === 'string' ? value : key;
    },
  };
}
