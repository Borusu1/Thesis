import { ApiWarehouseDataService } from '@/src/services/warehouse/ApiWarehouseDataService';
import { WarehouseDataService } from '@/src/services/warehouse/types';

let warehouseDataService: WarehouseDataService | null = null;

export function getWarehouseDataService(): WarehouseDataService {
  if (!warehouseDataService) {
    warehouseDataService = new ApiWarehouseDataService();
  }

  return warehouseDataService;
}
