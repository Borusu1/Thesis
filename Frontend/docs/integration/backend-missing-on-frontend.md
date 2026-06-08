# Backend functionality missing on Frontend

| Functionality | Current backend location | Why it was missing on frontend | Minimal fix | Status |
| --- | --- | --- | --- | --- |
| JWT login + `/auth/me` | `/api/v1/auth/*` | Frontend used a local temporary session | Implement `ApiWarehouseDataService` with token storage and `/auth/me` refresh | done |
| Product creation via API | `/api/v1/products` | Frontend created products in memory | Wire `AddProductScreen` to backend `POST /products` | done |
| Receipt flow by `tag_uid` | `/api/v1/inventory/receipts` | Frontend had no backend-backed receipt form | Add receipt form to product details | done |
| Partial/full shipments by `tag_uid` | `/api/v1/inventory/tags/*/shipments/*` | Frontend used generic stock actions | Add per-tag shipment controls in product details | done |
| Active tags endpoint | `/api/v1/inventory/tags/active` | Frontend stored tags locally in product model | Load active tags from backend and filter by `product_id` client-side | done |
| Tag history + tag reuse semantics | `/api/v1/inventory/tags/{tag_uid}/history` | Frontend did not model tag lifecycle | Use history for NFC lookup and reusable-tag state | done |
| UUID-based tag identity | `NFCTag.uid` | Frontend used temporary strings and raw scan IDs | Normalize all lookup/forms around UUID values | done |
| Global inventory events feed | `/api/v1/inventory/events` | Frontend history tab had only local operations | Switch history tab and dashboard recent actions to backend events | done |
| CORS for web client | FastAPI middleware | Web frontend could not call backend cross-origin | Add configurable backend CORS middleware | done |
| Development login account | app startup | Frontend had no real credentials to log in with | Add one backend development account for local sign-in | done |
