import { createContext, PropsWithChildren, useContext, useEffect, useMemo, useState } from 'react';

import { UserSession } from '@/src/types/app';
import { useWarehouseService } from '@/src/providers/WarehouseServiceProvider';

type AuthContextValue = {
  isReady: boolean;
  session: UserSession | null;
  login: (username: string, password: string) => Promise<void>;
  logout: () => Promise<void>;
};

const AuthContext = createContext<AuthContextValue | null>(null);

export function AuthProvider({ children }: PropsWithChildren) {
  const warehouseService = useWarehouseService();
  const [session, setSession] = useState<UserSession | null>(null);
  const [isReady, setIsReady] = useState(false);

  useEffect(() => {
    let isMounted = true;

    async function restoreSession() {
      try {
        const restoredSession = await warehouseService.getSession();

        if (isMounted) {
          setSession(restoredSession);
        }
      } finally {
        if (isMounted) {
          setIsReady(true);
        }
      }
    }

    restoreSession();

    return () => {
      isMounted = false;
    };
  }, [warehouseService]);

  const value = useMemo<AuthContextValue>(
    () => ({
      isReady,
      session,
      login: async (username: string, password: string) => {
        const nextSession = await warehouseService.login(username, password);
        setSession(nextSession);
      },
      logout: async () => {
        await warehouseService.logout();
        setSession(null);
      },
    }),
    [isReady, session, warehouseService]
  );

  return <AuthContext.Provider value={value}>{children}</AuthContext.Provider>;
}

export function useAuth() {
  const context = useContext(AuthContext);

  if (!context) {
    throw new Error('useAuth must be used within AuthProvider');
  }

  return context;
}
