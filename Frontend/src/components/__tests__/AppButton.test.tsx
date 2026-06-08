import { fireEvent, render } from '@testing-library/react-native';

import { AppButton } from '@/src/components/AppButton';

describe('AppButton', () => {
  it('renders its label', () => {
    const { getByText } = render(<AppButton label="Зберегти" onPress={() => {}} />);

    expect(getByText('Зберегти')).toBeTruthy();
  });

  it('calls onPress when pressed', () => {
    const onPress = jest.fn();
    const { getByText } = render(<AppButton label="Tap" onPress={onPress} />);

    fireEvent.press(getByText('Tap'));

    expect(onPress).toHaveBeenCalledTimes(1);
  });

  it('does not call onPress when disabled', () => {
    const onPress = jest.fn();
    const { getByText } = render(<AppButton disabled label="Tap" onPress={onPress} />);

    fireEvent.press(getByText('Tap'));

    expect(onPress).not.toHaveBeenCalled();
  });

  it('exposes the button accessibility role', () => {
    const { getByRole } = render(<AppButton label="Go" onPress={() => {}} />);

    expect(getByRole('button')).toBeTruthy();
  });

  it('renders the secondary variant', () => {
    const { getByText } = render(
      <AppButton label="Secondary" onPress={() => {}} variant="secondary" />
    );

    expect(getByText('Secondary')).toBeTruthy();
  });
});
