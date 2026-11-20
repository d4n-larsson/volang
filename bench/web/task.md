# Task: an inventory API

Write a vo program `main.vo` that serves a JSON API over plain HTTP on port 8765 of this machine (the serve
address "8765"), with 2 worker processes, until it is stopped. Keep
the items in the SQLite database `data/inventory.db` (create the table when it is missing). The program
may consist of several files; `main.vo` is the entry.

## Items

An item has an `id` (a whole number the server assigns, 1, 2, 3, ...), a `sku` (text), a `name` (text),
and a `quantity` (a whole number). Items are sent and returned as JSON objects with exactly these keys.

## Requests

| Request | Answer |
|---|---|
| `GET /items` | 200, a JSON array of every item, ordered by id |
| `POST /items` with `{"sku": ..., "name": ..., "quantity": ...}` | 201 with the new item and a `Location: /items/ID` header |
| `GET /items/ID` | 200 with the item |
| `PUT /items/ID` with `{"sku": ..., "name": ..., "quantity": ...}` | 200 with the changed item |
| `DELETE /items/ID` | 204, no body |
| `GET /items?min_quantity=N` | 200, the items whose quantity is at least N |

## Rules

- `sku`: 3 to 12 characters, upper-case letters and digits only; `name`: 1 to 50 characters; `quantity`:
  0 to 10000. A body that breaks a rule, misses a key, has a key of the wrong type or an extra key is
  answered 400 or 422 (either), and nothing changes.
- Two items never have the same `sku`: creating or changing an item to a sku another item has is 409.
- An ID that is not a whole number is 400; an item that does not exist is 404.
- A method the path does not support is 405; any other path is 404.
- Every answer with a body is JSON with `Content-Type: application/json` (an error's body may be any JSON
  object, `application/problem+json` is fine too).
- Requests send JSON bodies with `Content-Type: application/json`.
