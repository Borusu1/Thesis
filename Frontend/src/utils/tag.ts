const UUID_PATTERN =
  /\b[0-9a-f]{8}-[0-9a-f]{4}-[1-5][0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}\b/i;

export function normalizeTagUid(tagUid: string) {
  return tagUid.trim().toLowerCase();
}

export function extractTagUid(value: string) {
  const matchedUuid = value.match(UUID_PATTERN);

  return matchedUuid ? normalizeTagUid(matchedUuid[0]) : null;
}

export function isValidTagUid(value: string) {
  return extractTagUid(value) !== null;
}
