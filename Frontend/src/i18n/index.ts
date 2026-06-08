import { I18n } from 'i18n-js';

import { translations } from '@/src/i18n/translations';
import { LocaleCode } from '@/src/types/app';

export const i18n = new I18n(translations);

i18n.enableFallback = true;
i18n.defaultLocale = 'uk';

export function setI18nLocale(locale: LocaleCode) {
  i18n.locale = locale;
}
