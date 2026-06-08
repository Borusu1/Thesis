import { fireEvent, render } from '@testing-library/react-native';

import { AppInput } from '@/src/components/AppInput';

describe('AppInput', () => {
  it('renders its label and value', () => {
    const { getByText, getByDisplayValue } = render(
      <AppInput label="Назва" onChangeText={() => {}} value="Яблука" />
    );

    expect(getByText('Назва')).toBeTruthy();
    expect(getByDisplayValue('Яблука')).toBeTruthy();
  });

  it('calls onChangeText when the text changes', () => {
    const onChangeText = jest.fn();
    const { getByLabelText } = render(
      <AppInput label="Назва" onChangeText={onChangeText} value="" />
    );

    fireEvent.changeText(getByLabelText('Назва'), 'Банани');

    expect(onChangeText).toHaveBeenCalledWith('Банани');
  });

  it('renders an error message when provided', () => {
    const { getByText } = render(
      <AppInput error="Поле є обов’язковим." label="Назва" onChangeText={() => {}} value="" />
    );

    expect(getByText('Поле є обов’язковим.')).toBeTruthy();
  });

  it('renders the placeholder', () => {
    const { getByPlaceholderText } = render(
      <AppInput label="Назва" onChangeText={() => {}} placeholder="Введіть назву" value="" />
    );

    expect(getByPlaceholderText('Введіть назву')).toBeTruthy();
  });
});
