export type LocaleCode = 'uk' | 'en';

export type UserSession = {
  id: number;
  email: string;
  isActive: boolean;
  accessToken: string;
};

export type AppSettings = {
  language: LocaleCode;
};
