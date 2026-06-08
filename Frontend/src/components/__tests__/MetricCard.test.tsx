import { render } from '@testing-library/react-native';

import { MetricCard } from '@/src/components/MetricCard';

describe('MetricCard', () => {
  it('renders the label and value', () => {
    const { getByText } = render(<MetricCard label="Усього товарів" value="42" />);

    expect(getByText('Усього товарів')).toBeTruthy();
    expect(getByText('42')).toBeTruthy();
  });

  it('renders with a non-default tone', () => {
    const { getByText } = render(
      <MetricCard label="Закінчується" tone="warning" value="3" />
    );

    expect(getByText('Закінчується')).toBeTruthy();
    expect(getByText('3')).toBeTruthy();
  });
});
