# Quickstart: validating the PostgreSQL package

Prerequisite: the spec of research R1 is implemented (a package function can connect under a scoped
`net`). A container runtime with `postgres:15` to `postgres:18` images already present, or a server
named in `VO_PG_TEST=host:port:database:user:password`.

## 1. Between edits

```sh
make quick AREA=postgres          # package test items, scripted peer, parity, docs
```

Expected: every group of `tests/postgres.py` passes; no server needed.

## 2. A program against a real server

```sh
docker run -d --name vo-pg -e POSTGRES_PASSWORD=secret -p 127.0.0.1:54329:5432 postgres:17
mkdir /tmp/pgdemo && cd /tmp/pgdemo
# main.vo: connect to 127.0.0.1:54329 with plain_text: true, create a table, insert with ?, query, print
vo package main.vo postgres /path/to/volang/packages/postgres
vo run main.vo                    # prints the rows
vo run --record run.jsonl main.vo
docker stop vo-pg
vo replay run.jsonl               # same output, no server
grep -c secret run.jsonl          # 0
```

Expected: the add output lists `net` and `entropy`; `main` declares `net("127.0.0.1:54329")` and
checks (R1); changing the port in the options to one outside the grant stops with R0010.

## 3. The move (SC-002)

```sh
python3 tests/postgres.py --group move --server
```

Expected: `examples/notes-api` and `tests/postgres/notes-api` differ only in imports, the functions
that open the database, effects and SQL; both answer the same HTTP requests the same way.

## 4. Once per change, and the gate

```sh
make test                         # includes tests/postgres.py (scripted peer; skip line for --server without one)
python3 tests/postgres.py --server --versions 15,16,17,18 --time    # in gate-step: SC-001, SC-004, SC-007, SC-008
git diff --stat BASE -- src std third_party docs/reference.md        # empty: SC-009
```

Details: [contracts/client.md](contracts/client.md), [data-model.md](data-model.md), [research.md](research.md).
