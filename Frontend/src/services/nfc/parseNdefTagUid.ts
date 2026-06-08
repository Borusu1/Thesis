import { extractTagUid } from '@/src/utils/tag';

type NdefRecord = {
  payload?: number[] | Uint8Array | string | null;
};

type NdefTagLike = {
  ndefMessage?: NdefRecord[] | null;
};

function decodePayload(payload: NdefRecord['payload']) {
  if (typeof payload === 'string') {
    return payload;
  }

  if (payload instanceof Uint8Array) {
    return new TextDecoder().decode(payload);
  }

  if (Array.isArray(payload)) {
    const bytes = Uint8Array.from(payload);

    if (bytes.length > 1) {
      const languageCodeLength = bytes[0] & 0x3f;
      const textPayload = bytes.slice(languageCodeLength + 1);

      return new TextDecoder().decode(textPayload.length ? textPayload : bytes);
    }

    return new TextDecoder().decode(bytes);
  }

  return '';
}

export function extractTagUidFromNdefMessage(tag: NdefTagLike | null | undefined) {
  for (const record of tag?.ndefMessage ?? []) {
    const extractedTagUid = extractTagUid(decodePayload(record.payload));

    if (extractedTagUid) {
      return extractedTagUid;
    }
  }

  return null;
}
