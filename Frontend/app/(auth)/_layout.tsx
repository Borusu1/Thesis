import { Redirect, Stack } from 'expo-router';

import { AppLoadingScreen } from '@/src/components/AppLoadingScreen';
import { useAuth } from '@/src/providers/AuthProvider';
import { useI18n } from '@/src/providers/LocaleProvider';

export default function AuthLayout() {
  const { isReady: authReady, session } = useAuth();
  const { isReady: localeReady } = useI18n();

  if (!authReady || !localeReady) {
    return <AppLoadingScreen />;
  }

  if (session) {
    return <Redirect href="/(app)/(tabs)/dashboard" />;
  }

  return <Stack screenOptions={{ headerShown: false }} />;
}
