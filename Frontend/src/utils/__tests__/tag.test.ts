import { extractTagUid, isValidTagUid, normalizeTagUid } from '@/src/utils/tag';

const VALID_UUID = '123e4567-e89b-12d3-a456-426614174000';

describe('tag utils', () => {
  describe('normalizeTagUid', () => {
    it('trims surrounding whitespace and lowercases', () => {
      expect(normalizeTagUid('  ABC-Def  ')).toBe('abc-def');
    });

    it('leaves an already normalized value unchanged', () => {
      expect(normalizeTagUid(VALID_UUID)).toBe(VALID_UUID);
    });
  });

  describe('extractTagUid', () => {
    it('extracts a UUID embedded inside other text', () => {
      expect(extractTagUid(`tag uid is ${VALID_UUID} ok`)).toBe(VALID_UUID);
    });

    it('normalizes the extracted UUID to lowercase', () => {
      expect(extractTagUid(VALID_UUID.toUpperCase())).toBe(VALID_UUID);
    });

    it('returns null when no UUID is present', () => {
      expect(extractTagUid('no uuid here')).toBeNull();
    });

    it('returns null for a malformed UUID', () => {
      expect(extractTagUid('123e4567-e89b-72d3-a456-426614174000')).toBeNull();
    });

    it('returns null for an empty string', () => {
      expect(extractTagUid('')).toBeNull();
    });
  });

  describe('isValidTagUid', () => {
    it('accepts a valid UUID', () => {
      expect(isValidTagUid(VALID_UUID)).toBe(true);
    });

    it('accepts a valid UUID wrapped in text', () => {
      expect(isValidTagUid(`uid=${VALID_UUID}`)).toBe(true);
    });

    it('rejects an arbitrary token', () => {
      expect(isValidTagUid('TAG-001')).toBe(false);
    });
  });
});
