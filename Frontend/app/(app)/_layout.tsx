import { Redirect, Stack } from 'expo-router';

import { AppLoadingScreen } from '@/src/components/AppLoadingScreen';
import { useAuth } from '@/src/providers/AuthProvider';
import { useI18n } from '@/src/providers/LocaleProvider';

export default function ProtectedLayout() {
  const { isReady: authReady, session } = useAuth();
  const { isReady: localeReady } = useI18n();

  if (!authReady || !localeReady) {
    return <AppLoadingScreen />;
  }

  if (!session) {
    return <Redirect href="/(auth)/login" />;
  }

  return (
    <Stack screenOptions={{ headerShown: false }}>
      <Stack.Screen name="(tabs)" />
      <Stack.Screen name="product/[productId]" />
    </Stack>
  );
}
