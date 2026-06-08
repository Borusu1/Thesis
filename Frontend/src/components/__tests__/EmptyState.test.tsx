import { render } from '@testing-library/react-native';

import { EmptyState } from '@/src/components/EmptyState';

describe('EmptyState', () => {
  it('renders the title and description', () => {
    const { getByText } = render(
      <EmptyState description="Список порожній" title="Нічого не знайдено" />
    );

    expect(getByText('Нічого не знайдено')).toBeTruthy();
    expect(getByText('Список порожній')).toBeTruthy();
  });
});
