import { createContext, PropsWithChildren, useContext, useMemo } from 'react';

import { getWarehouseDataService } from '@/src/services/warehouse/createWarehouseService';
import { WarehouseDataService } from '@/src/services/warehouse/types';

const WarehouseServiceContext = createContext<WarehouseDataService | null>(null);

export function WarehouseServiceProvider({ children }: PropsWithChildren) {
  const service = useMemo(() => getWarehouseDataService(), []);

  return (
    <WarehouseServiceContext.Provider value={service}>{children}</WarehouseServiceContext.Provider>
  );
}

export function useWarehouseService() {
  const context = useContext(WarehouseServiceContext);

  if (!context) {
    throw new Error('useWarehouseService must be used within WarehouseServiceProvider');
  }

  return context;
}
