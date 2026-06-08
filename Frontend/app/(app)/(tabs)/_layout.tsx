import { Ionicons } from '@expo/vector-icons';
import { Tabs } from 'expo-router';

import { useI18n } from '@/src/providers/LocaleProvider';
import { colors } from '@/src/theme';

type IoniconName = React.ComponentProps<typeof Ionicons>['name'];

function tabIcon(name: IoniconName, outlineName: IoniconName) {
  return ({ color, focused }: { color: string; focused: boolean }) => (
    <Ionicons color={color} name={focused ? name : outlineName} size={22} />
  );
}

export default function TabsLayout() {
  const { t } = useI18n();

  return (
    <Tabs
      screenOptions={{
        headerShown: false,
        tabBarActiveTintColor: colors.primary,
        tabBarInactiveTintColor: colors.textMuted,
        tabBarStyle: {
          backgroundColor: colors.surface,
          borderTopColor: colors.border,
        },
        tabBarLabelStyle: {
          fontSize: 12,
          fontWeight: '600',
        },
      }}
    >
      <Tabs.Screen
        name="dashboard"
        options={{ title: t('dashboardTab'), tabBarIcon: tabIcon('grid', 'grid-outline') }}
      />
      <Tabs.Screen
        name="inventory"
        options={{ title: t('inventoryTab'), tabBarIcon: tabIcon('cube', 'cube-outline') }}
      />
      <Tabs.Screen
        name="add-product"
        options={{ title: t('addProductTab'), tabBarIcon: tabIcon('add-circle', 'add-circle-outline') }}
      />
      <Tabs.Screen
        name="history"
        options={{ title: t('historyTab'), tabBarIcon: tabIcon('time', 'time-outline') }}
      />
      <Tabs.Screen
        name="nfc"
        options={{ title: t('nfcTab'), tabBarIcon: tabIcon('scan', 'scan-outline') }}
      />
      <Tabs.Screen
        name="settings"
        options={{ title: t('settingsTab'), tabBarIcon: tabIcon('settings', 'settings-outline') }}
      />
    </Tabs>
  );
}
