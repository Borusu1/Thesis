import { createContext, PropsWithChildren, useContext, useEffect, useMemo, useState } from 'react';

import { i18n, setI18nLocale } from '@/src/i18n';
import { useWarehouseService } from '@/src/providers/WarehouseServiceProvider';
import { LocaleCode } from '@/src/types/app';

type LocaleContextValue = {
  isReady: boolean;
  locale: LocaleCode;
  setLocale: (locale: LocaleCode) => Promise<void>;
  t: typeof i18n.t;
};

const LocaleContext = createContext<LocaleContextValue | null>(null);

export function LocaleProvider({ children }: PropsWithChildren) {
  const warehouseService = useWarehouseService();
  const [locale, setLocaleState] = useState<LocaleCode>('uk');
  const [isReady, setIsReady] = useState(false);

  useEffect(() => {
    let isMounted = true;

    async function restoreLocale() {
      try {
        const settings = await warehouseService.getSettings();
        const nextLocale = settings.language;

        setI18nLocale(nextLocale);

        if (isMounted) {
          setLocaleState(nextLocale);
        }
      } finally {
        if (isMounted) {
          setIsReady(true);
        }
      }
    }

    restoreLocale();

    return () => {
      isMounted = false;
    };
  }, [warehouseService]);

  const value = useMemo<LocaleContextValue>(
    () => ({
      isReady,
      locale,
      setLocale: async (nextLocale: LocaleCode) => {
        setI18nLocale(nextLocale);
        await warehouseService.updateSettings({ language: nextLocale });
        setLocaleState(nextLocale);
      },
      t: i18n.t.bind(i18n),
    }),
    [isReady, locale, warehouseService]
  );

  return <LocaleContext.Provider value={value}>{children}</LocaleContext.Provider>;
}

export function useI18n() {
  const context = useContext(LocaleContext);

  if (!context) {
    throw new Error('useI18n must be used within LocaleProvider');
  }

  return context;
}
