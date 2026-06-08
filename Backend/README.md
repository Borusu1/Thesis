# Warehouse Backend

Цей проєкт є центральним сервером складської системи. Він зберігає товари, NFC-мітки, рух товару, події синхронізації від `ESP32-S3` пристрою та дані для web-інтерфейсу.

Прочитавши цей файл, можна зрозуміти:
- які сутності існують у системі;
- як працює web-клієнт;
- як працює `ESP32` пристрій;
- які API вже реалізовані;
- що саме вважається джерелом істини в системі.

## Роль бекенду в системі

Бекенд виконує 4 основні задачі:

1. Зберігає довідник товарів.
2. Веде життєвий цикл NFC-міток і їх історію використання.
3. Приймає складські операції:
   - надходження;
   - часткове відвантаження;
   - повне відвантаження;
   - provision / reissue NFC-мітки.
4. Обслуговує два типи клієнтів:
   - web frontend;
   - `ESP32-S3` device.

## Поточна бізнес-модель

### Товар

Товар має:
- `id`
- `sku`
- `name`
- `description`
- `quantity_on_hand`

`quantity_on_hand` рахується на рівні всього товару по всіх активних usages.

### NFC-мітка

Кожна NFC-мітка має:
- логічний `tag_uid` (`UUID`, записаний у NDEF payload на самій мітці);
- фізичний `chip_uid_hex` (UID самого NFC-чіпа);
- дату provision;
- device, який її provision-ив.

Система розрізняє:
- логічну ідентичність мітки: `tag_uid`;
- фізичну ідентичність чіпа: `chip_uid_hex`.

Це важливо для `reissue`:
- якщо той самий фізичний чіп перепризначається новому `UUID`,
- бекенд може трактувати це як `auto reissue`, якщо на старому usage немає активного товару.

### Використання мітки (`tag usage`)

Основна модель зараз така:
- одна NFC-мітка представляє одну партію одного товару;
- на мітці зберігається один товар і кількість від `1` до `99999`;
- кількість на мітці може зменшуватись частковими відвантаженнями;
- коли кількість стає `0`, usage вважається закритим.

Отже, мітка більше не означає “1 мітка = 1 одиниця”, а означає:
- `1 мітка = 1 lot одного товару з поточною кількістю`.

## Як працює система в цілому

### 1. Provision мітки

На пристрої:
- blank або вже використана мітка записується новим `UUID`;
- пристрій reread-ить payload і перевіряє запис;
- після цього робить `provision sync` у backend.

На бекенді:
- створюється або оновлюється запис `nfc_tags`;
- за потреби відбувається `auto reissue` по `chip_uid_hex`;
- якщо старий usage ще активний, бекенд повертає `409 conflict`.

### 2. Receipt

На пристрої:
- вибирається товар;
- вводиться кількість;
- сканується provisioned tag;
- локально створюється операція receipt;
- потім вона синхронізується в backend.

На бекенді:
- створюється новий active usage для цієї мітки;
- кількість додається до товару;
- створюється inventory event.

### 3. Shipment

На пристрої:
- сканується мітка;
- пристрій читає поточну кількість;
- оператор вводить кількість до відвантаження;
- якщо вона менша за залишок, це `partial shipment`;
- якщо дорівнює залишку, це `full shipment`.

На бекенді:
- зменшується `quantity_current`;
- якщо залишок став `0`, usage закривається;
- пишеться inventory event.

### 4. Web analytics

Web-клієнт працює поверх цього ж backend і читає:
- список товарів;
- історію операцій;
- активні теги;
- історію конкретної мітки;
- lookup мітки.

## Device-only авторизація

Система зараз працює без паролів і без badge login.

Поточна схема:
- device авторизується по власному `ESP32 hardware id`;
- бекенд ідентифікує операції через `actor_device_id`;
- `actor_user_id` для device-синхронізації зараз `null`.

Це означає:
- видно, який саме пристрій зробив зміну;
- персональний аудит по конкретній людині зараз не ведеться.

## Основні API, які вже реалізовані

### Web API

Авторизація:
- `POST /api/v1/auth/login`
- `GET /api/v1/auth/me`

Товари:
- `GET /api/v1/products`
- `GET /api/v1/products/{product_id}`
- `POST /api/v1/products`

Склад:
- `POST /api/v1/inventory/receipts`
- `GET /api/v1/inventory/events`
- `GET /api/v1/inventory/tags/active`
- `GET /api/v1/inventory/tags/{tag_uid}/history`
- `POST /api/v1/inventory/tags/{tag_uid}/shipments/partial`
- `POST /api/v1/inventory/tags/{tag_uid}/shipments/full`

### Device API

Авторизація пристрою:
- `POST /api/v1/device/auth/login`

Каталог:
- `GET /api/v1/device/products/recent`
- `GET /api/v1/device/products/search?sku=...`

Lookup і мітки:
- `GET /api/v1/device/tags/{tag_uid}/lookup`
- `POST /api/v1/device/tags/provision`
- `POST /api/v1/device/tags/reissue`

Синхронізація операцій:
- `POST /api/v1/device/operations/sync`

Health:
- `GET /api/v1/health`

## Що зберігається в БД

Ключові таблиці:
- `users`
- `devices`
- `products`
- `nfc_tags`
- `tag_usages`
- `inventory_events`

Коротко:
- `products` зберігає довідник товарів;
- `nfc_tags` зберігає provisioned NFC-мітки;
- `tag_usages` зберігає активні й завершені використання міток;
- `inventory_events` зберігає повну історію руху.

## Джерело істини

Джерело істини для централізованого стану складу:
- backend PostgreSQL.

Пристрій працює як `offline-first` клієнт:
- спочатку пише локально;
- потім синхронізує в backend.

Але фінальний агрегований стан складу визначається саме сервером.

## Запуск локально

```bash
python -m venv .venv
source .venv/bin/activate
pip install -e ".[dev]"
alembic upgrade head
uvicorn app.main:app --reload
```

## Запуск через Docker

```bash
docker compose up --build
```

## Тести

```bash
pytest
```

Є також окремий сценарій для ізольованих інтеграційних тестів у тимчасовій Postgres БД:

```bash
./scripts/run_ephemeral_postgres_tests.sh
```

## Demo user

У dev-середовищі застосунок може автоматично створювати demo user:

- email: `demo@example.com`
- password: `demo123`

Це керується env-змінними:
- `SEED_DEMO_USER`
- `DEMO_USER_EMAIL`
- `DEMO_USER_PASSWORD`

## Структура проєкту

```text
Backend
├── alembic
│   ├── env.py
│   └── versions
├── app
│   ├── api
│   │   ├── dependencies
│   │   ├── router.py
│   │   └── v1/endpoints
│   ├── core
│   ├── db
│   ├── models
│   ├── schemas
│   ├── services
│   └── main.py
├── tests
├── scripts
├── Dockerfile
├── docker-compose.yml
├── alembic.ini
└── pyproject.toml
```

## Підсумок

На поточний момент бекенд уже повністю покриває:
- каталог товарів;
- provision і reissue NFC-міток;
- receipt / partial shipment / full shipment;
- sync від `ESP32-S3`;
- web lookup та аналітичне читання історії.

Тобто це вже не каркас, а робочий центральний сервер складської системи.
