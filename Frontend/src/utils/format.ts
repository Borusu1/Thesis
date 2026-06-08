import { LocaleCode } from '@/src/types/app';
import { Operation, OperationType } from '@/src/types/warehouse';

export function formatDateTime(value: string, locale: LocaleCode) {
  return new Intl.DateTimeFormat(locale, {
    day: '2-digit',
    month: '2-digit',
    year: 'numeric',
    hour: '2-digit',
    minute: '2-digit',
  }).format(new Date(value));
}

export function formatQuantity(quantity: number, unit = 'шт') {
  return `${quantity} ${unit}`;
}

export function formatSignedQuantity(quantity: number, unit = 'шт') {
  const sign = quantity > 0 ? '+' : '';

  return `${sign}${quantity} ${unit}`;
}

export function resolveOperationDelta(operation: Operation) {
  return operation.type === 'receipt' ? operation.quantity : -operation.quantity;
}

export function formatOperationTypeLabel(
  operationType: OperationType,
  t: (key: string) => string
) {
  return t(`operationType.${operationType}`);
}
