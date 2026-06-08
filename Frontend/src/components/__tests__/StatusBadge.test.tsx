import { render } from '@testing-library/react-native';

import { StatusBadge } from '@/src/components/StatusBadge';
import { useI18n } from '@/src/providers/LocaleProvider';
import { createMockI18n } from '@/test-utils/mockI18n';

jest.mock('@/src/providers/LocaleProvider', () => ({
  useI18n: jest.fn(),
}));

const mockedUseI18n = jest.mocked(useI18n);

describe('StatusBadge', () => {
  beforeEach(() => {
    mockedUseI18n.mockReturnValue(createMockI18n() as never);
  });

  it('renders the inStock label', () => {
    const { getByText } = render(<StatusBadge status="inStock" />);

    expect(getByText('В наявності')).toBeTruthy();
  });

  it('renders the outOfStock label', () => {
    const { getByText } = render(<StatusBadge status="outOfStock" />);

    expect(getByText('Немає')).toBeTruthy();
  });
});
