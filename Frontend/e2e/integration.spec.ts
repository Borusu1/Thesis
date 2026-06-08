import { expect, test } from '@playwright/test';

test('runs the core warehouse lifecycle in web UI', async ({ page }) => {
  const productName = `Яблука ${Date.now()}`;
  const tagUid = crypto.randomUUID();
  const signInButton = /^(Sign in|Увійти)$/i;
  const connectedToApiText = /(Connected to API|Підключено до API)/i;
  const addTab = /^(Add|Додати)$/i;
  const namePlaceholder = /(For example, Temperature sensor|Наприклад, Температурний сенсор)/i;
  const descriptionPlaceholder = /(Short product description|Короткий опис товару)/i;
  const tagPlaceholder = /123e4567-e89b-12d3-a456-426614174000/i;
  const quantityLabel = /^(Quantity|Кількість)$/i;
  const receiptAction = /^(Create receipt|Додати прихід)$/i;
  const partialShipmentAction = /^(Partial shipment|Часткове відвантаження)$/i;
  const fullShipmentAction = /^(Full shipment|Повне відвантаження)$/i;
  const noActiveTagsText = /(No active tags|Активних міток немає)/i;
  const findTagAction = /^(Find tag|Знайти мітку)$/i;
  const tagAvailableText = /(Tag is available|Мітка вільна)/i;
  const activeProductFoundText = /(Active product found|Активний товар знайдено)/i;

  await page.goto('/');

  await page.getByRole('button', { name: signInButton }).click();
  await expect(page.getByText(connectedToApiText)).toBeVisible();

  await page.getByText(addTab).click();
  await page.getByPlaceholder(namePlaceholder).fill(productName);
  await page.getByPlaceholder(descriptionPlaceholder).fill('Інтеграційний e2e товар');
  await page.getByRole('button', { name: /^(Create product|Створити товар)$/i }).click();

  await expect(page.getByText(productName).first()).toBeVisible();

  await page.getByPlaceholder(tagPlaceholder).fill(tagUid);
  await page.getByLabel(quantityLabel).fill('3');
  await page.getByPlaceholder('A-01').fill('A-01');
  await page.getByRole('button', { name: receiptAction }).click();

  await expect(page.getByText(tagUid).last()).toBeVisible();
  await expect(page.getByText(/3 (pcs|шт)/i).first()).toBeVisible();

  await page.getByRole('button', { name: partialShipmentAction }).click();
  await expect(page.getByText(/2 (pcs|шт)/i).first()).toBeVisible();

  await page.getByRole('button', { name: fullShipmentAction }).click();
  await expect(page.getByText(noActiveTagsText).first()).toBeVisible();
  await expect(page.getByText(fullShipmentAction).first()).toBeVisible();

  await page.goto('/nfc');
  await page.getByPlaceholder(tagPlaceholder).fill(tagUid);
  await page.getByRole('button', { name: findTagAction }).click();
  await expect(page.getByText(tagAvailableText).first()).toBeVisible();

  await page.goto('/inventory');
  await page.getByText(productName).click();
  await page.getByPlaceholder(tagPlaceholder).fill(tagUid);
  await page.getByLabel(quantityLabel).fill('2');
  await page.getByRole('button', { name: receiptAction }).click();

  await page.goto('/nfc');
  await page.getByPlaceholder(tagPlaceholder).fill(tagUid);
  await page.getByRole('button', { name: findTagAction }).click();
  await expect(page.getByText(activeProductFoundText).first()).toBeVisible();
  await expect(page.getByText(productName).first()).toBeVisible();
});
