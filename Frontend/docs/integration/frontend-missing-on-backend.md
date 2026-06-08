# Frontend functionality missing on Backend

| Functionality | Current frontend location | Why it does not fit backend | Minimal fix | Status |
| --- | --- | --- | --- | --- |
| Prefilled login credentials | `src/features/auth/screens/LoginScreen.tsx` | Backend uses email + password and stores users in DB | Remove prefilled values and use standard email/password entry | done |
| Legacy product shape with `SKU/category/unit/location/minStock` | old inventory/product screens | Backend `Product` only exposes `name`, `description`, `quantity_on_hand`, `created_at` | Simplify UI to backend shape for the first integrated version | done |
| Generic `stock-in/stock-out/adjustment` actions | old `ProductDetailsScreen` | Backend supports `receipt`, `shipment_partial`, `shipment_full` by `tag_uid` | Replace UI actions with receipt and shipment forms | done |
| Frontend-only `getDashboardSummary` | previous local service | Backend has no summary endpoint | Compose dashboard client-side from `/products` + `/inventory/events` | done |
| Frontend-only `getOperations` model | previous local service | Backend had no global operations list | Add backend read-only `/inventory/events` and map events on frontend | done |
| Raw NFC hardware tag ID lookup | old `nfcScanner.ts` | Backend works with UUID tag identity, not device-specific raw UID | Read UUID from NDEF payload and use manual UUID input on web | done |
| Temporary local-mode copy | dashboard/settings translations | UI claimed data was local and temporary | Replace with API-connected copy | done |
| Product low-stock logic | old inventory/dashboard UI | Backend has no `minStock` field | Drop `lowStock` for integrated MVP and keep `inStock/outOfStock` | done |
