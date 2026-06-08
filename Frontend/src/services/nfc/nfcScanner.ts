import { Platform } from 'react-native';

import { extractTagUidFromNdefMessage } from '@/src/services/nfc/parseNdefTagUid';

let hasStarted = false;

export async function scanNfcTagId() {
  if (Platform.OS === 'web') {
    throw new Error('NFC_WEB_UNAVAILABLE');
  }

  const { default: NfcManager, NfcTech } = await import('react-native-nfc-manager');

  if (!hasStarted) {
    await NfcManager.start();
    hasStarted = true;
  }

  const supported = await NfcManager.isSupported();

  if (!supported) {
    throw new Error('NFC_NOT_SUPPORTED');
  }

  const enabled = await NfcManager.isEnabled();

  if (!enabled) {
    throw new Error('NFC_DISABLED');
  }

  const technologies =
    Platform.OS === 'ios'
      ? [NfcTech.Ndef, NfcTech.Iso15693IOS, NfcTech.MifareIOS, NfcTech.FelicaIOS]
      : [
          NfcTech.Ndef,
          NfcTech.NfcA,
          NfcTech.NfcV,
          NfcTech.IsoDep,
          NfcTech.MifareClassic,
          NfcTech.MifareUltralight,
        ];

  try {
    await NfcManager.requestTechnology(technologies, {
      alertMessage: 'Hold the device near a warehouse tag.',
    });

    const tag = await NfcManager.getTag();
    const normalizedTagUid = extractTagUidFromNdefMessage(tag);

    if (!normalizedTagUid) {
      throw new Error('NFC_TAG_ID_NOT_FOUND');
    }

    return normalizedTagUid;
  } finally {
    try {
      await NfcManager.cancelTechnologyRequest();
    } catch {
      // Ignore cleanup errors after session closure.
    }
  }
}
