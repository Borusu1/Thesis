# Warehouse Frontend

Цей проєкт є web/mobile клієнтом складської системи. Він працює поверх backend API, показує стан товарів, історію руху, lookup NFC-міток і частину операцій керування складом.

Прочитавши цей файл, можна зрозуміти:
- які екрани вже реалізовані;
- які дані frontend читає з backend;
- які дії користувач може виконувати;
- як frontend пов’язаний із `ESP32` пристроєм.

## Роль фронтенду в системі

Frontend потрібен для двох задач:

1. Дати людині зручний інтерфейс до backend.
2. Показати аналітику та поточний складський стан без роботи з самим пристроєм.

Сайт не спілкується з NFC-напряму на сервері. Він або:
- читає дані з backend;
- або на мобільному пристрої може локально просканувати NFC-мітку й потім зробити lookup через backend.

## Як frontend працює зараз

Frontend побудований на `Expo Router` і використовує backend як основне джерело даних.

Поточний сценарій роботи такий:
- користувач логіниться через `POST /api/v1/auth/login`;
- frontend зберігає user session;
- далі всі екрани читають дані з backend;
- для операцій зміни стану використовуються відповідні API backend.

## Основні реалізовані екрани

### Login

Екран входу:
- приймає email і password;
- логінить користувача;
- підтягує `/auth/me`;
- зберігає сесію локально.

### Dashboard

Головний аналітичний екран показує:
- кількість товарів;
- загальну кількість одиниць;
- кількість `out of stock`;
- останні складські рухи;
- блок `Needs attention`.

`Needs attention` зараз формується на основі поточного inventory summary у frontend-моделі.

### Inventory

Екран inventory показує:
- список товарів;
- пошук по назві, опису та id;
- базові метрики товару;
- перехід у картку товару.

### Product Details

Картка товару показує:
- базову інформацію про товар;
- кількість;
- активні теги цього товару;
- історію операцій по товару.

Також зараз у картці товару ще є mutating-дії:
- `partial shipment`
- `full shipment`

Тобто frontend на даний момент не є строго `read-only analytics`, а все ще містить частину операційного UI.

### Add Product

Є окремий екран створення товару:
- `sku`
- `name`
- `description`

Після створення відбувається перехід у картку товару.

### History

Екран історії показує:
- усі inventory events у хронологічному порядку;
- тип операції;
- кількість;
- товар;
- мітку;
- джерело операції.

Для подій із девайса frontend показує `actor = Device`.

### NFC Lookup

Екран NFC працює у двох режимах:

1. На web:
- ручний lookup по `tag UUID`.

2. На мобільному пристрої:
- сканування NFC;
- витягування `UUID` з NDEF payload;
- lookup активного usage через backend.

Екран показує:
- знайдений активний товар;
- або стан “мітка вільна”;
- або стан “мітка не знайдена”.

### Settings

Є екран налаштувань:
- мова;
- параметри сесії;
- базові app settings.

## Як frontend пов’язаний із backend

Основний інтеграційний шар:
- [ApiWarehouseDataService.ts](/Users/bohdan/Code/Courcework/Frontend/src/services/warehouse/ApiWarehouseDataService.ts)

Він уже покриває:
- auth;
- products;
- history;
- active tags;
- tag history;
- tag lookup;
- partial/full shipment;
- create product.

Основні backend endpoints, які frontend реально використовує:
- `POST /api/v1/auth/login`
- `GET /api/v1/auth/me`
- `GET /api/v1/products`
- `GET /api/v1/products/{id}`
- `POST /api/v1/products`
- `GET /api/v1/inventory/events`
- `GET /api/v1/inventory/tags/active`
- `GET /api/v1/inventory/tags/{tag_uid}/history`
- `POST /api/v1/inventory/tags/{tag_uid}/shipments/partial`
- `POST /api/v1/inventory/tags/{tag_uid}/shipments/full`

## Як frontend пов’язаний із пристроєм

Frontend не спілкується з `ESP32` напряму.

Зв’язок іде тільки через backend:
- `ESP32` синхронізує receipt/shipment/provision;
- backend зберігає ці зміни;
- frontend потім показує оновлений стан складу.

Отже, frontend є візуалізацією того, що:
- зроблено через web;
- або синхронізовано з пристрою.

## Поточний стан з точки зору продукту

На цей момент frontend уже дає змогу:
- увійти в систему;
- подивитись загальний стан складу;
- переглянути товари;
- перейти в деталі товару;
- подивитись історію руху;
- знайти мітку через lookup;
- виконати часткове або повне відвантаження;
- створити новий товар.

Тобто сайт зараз є змішаним:
- частково аналітичним;
- частково операційним.

## Локальний запуск

```bash
npm install
npx expo start
```

Для web:

```bash
npx expo start --web
```

## Production build

Є Dockerfile для web deployment:

```bash
docker build \
  --build-arg EXPO_PUBLIC_API_BASE_URL=https://your-backend-domain.example \
  -t warehouse-frontend .
```

Контейнер збирає статичний web export і віддає його через `nginx`.

## Важливі технічні рішення

- роутинг: `Expo Router`
- дані: backend API
- локальна сесія: `AsyncStorage`
- NFC lookup: mobile scan + backend lookup
- web deploy: статичний export через `nginx`

## Структура проєкту

```text
Frontend
├── app
│   ├── (auth)
│   ├── (app)
│   │   ├── (tabs)
│   │   └── product
│   ├── +html.tsx
│   └── _layout.tsx
├── src
│   ├── components
│   ├── features
│   │   ├── auth
│   │   ├── dashboard
│   │   ├── history
│   │   ├── inventory
│   │   ├── nfc
│   │   └── settings
│   ├── providers
│   ├── services
│   ├── theme
│   ├── types
│   └── utils
├── Dockerfile
├── nginx.conf
└── package.json
```

## Підсумок

На поточний момент frontend уже дозволяє повноцінно пояснити стан системи людині:
- які товари існують;
- які теги активні;
- які операції були виконані;
- який поточний стан конкретної NFC-мітки;
- які зміни прийшли з `ESP32` пристрою.

Це вже робочий клієнт поверх warehouse backend, а не тільки демонстраційний інтерфейс.
