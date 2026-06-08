import { extractTagUidFromNdefMessage } from '@/src/services/nfc/parseNdefTagUid';

describe('extractTagUidFromNdefMessage', () => {
  it('extracts UUID from a text NDEF payload', () => {
    const text = '123e4567-e89b-12d3-a456-426614174000';
    const bytes = [0x02, 0x65, 0x6e, ...Array.from(new TextEncoder().encode(text))];

    expect(
      extractTagUidFromNdefMessage({
        ndefMessage: [{ payload: bytes }],
      })
    ).toBe('123e4567-e89b-12d3-a456-426614174000');
  });

  it('returns null when payload does not contain UUID', () => {
    expect(
      extractTagUidFromNdefMessage({
        ndefMessage: [{ payload: [0x02, 0x65, 0x6e, 0x54, 0x41, 0x47] }],
      })
    ).toBeNull();
  });
});
