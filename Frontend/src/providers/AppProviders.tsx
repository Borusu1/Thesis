import { PropsWithChildren } from 'react';

import { AuthProvider } from '@/src/providers/AuthProvider';
import { LocaleProvider } from '@/src/providers/LocaleProvider';
import { WarehouseServiceProvider } from '@/src/providers/WarehouseServiceProvider';

export function AppProviders({ children }: PropsWithChildren) {
  return (
    <WarehouseServiceProvider>
      <LocaleProvider>
        <AuthProvider>{children}</AuthProvider>
      </LocaleProvider>
    </WarehouseServiceProvider>
  );
}
