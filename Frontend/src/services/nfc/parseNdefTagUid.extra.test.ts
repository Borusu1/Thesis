import { extractTagUidFromNdefMessage } from '@/src/services/nfc/parseNdefTagUid';

const UUID = '123e4567-e89b-12d3-a456-426614174000';

describe('extractTagUidFromNdefMessage (extra cases)', () => {
  it('extracts UUID from a plain string payload', () => {
    expect(
      extractTagUidFromNdefMessage({ ndefMessage: [{ payload: `uid:${UUID}` }] })
    ).toBe(UUID);
  });

  it('extracts UUID from a Uint8Array payload', () => {
    const payload = new TextEncoder().encode(UUID);

    expect(extractTagUidFromNdefMessage({ ndefMessage: [{ payload }] })).toBe(UUID);
  });

  it('scans multiple records and returns the first matching UUID', () => {
    expect(
      extractTagUidFromNdefMessage({
        ndefMessage: [{ payload: 'no uuid' }, { payload: UUID }],
      })
    ).toBe(UUID);
  });

  it('returns null for a null tag', () => {
    expect(extractTagUidFromNdefMessage(null)).toBeNull();
  });

  it('returns null for an undefined tag', () => {
    expect(extractTagUidFromNdefMessage(undefined)).toBeNull();
  });

  it('returns null when ndefMessage is empty', () => {
    expect(extractTagUidFromNdefMessage({ ndefMessage: [] })).toBeNull();
  });

  it('returns null when a record payload is null', () => {
    expect(extractTagUidFromNdefMessage({ ndefMessage: [{ payload: null }] })).toBeNull();
  });

  it('handles a language-code prefixed text record', () => {
    const text = `prefix ${UUID}`;
    const bytes = [0x02, 0x65, 0x6e, ...Array.from(new TextEncoder().encode(text))];

    expect(extractTagUidFromNdefMessage({ ndefMessage: [{ payload: bytes }] })).toBe(UUID);
  });
});
