# Compression backend

The mobile client sends `multipart/form-data` to `POST /api/compression/jobs` with field `video`. The API uploads the source to Cloudinary, records a durable PostgreSQL `QUEUED` job, and returns `202`. A separate worker claims jobs, downloads source media to temporary storage, runs the C++/FFmpeg compressor, uploads the result to Cloudinary, and records `COMPLETED` or `FAILED`. The existing `GET /api/compression/jobs/:jobId` response contract is preserved.

PostgreSQL is the queue for this MVP. `FOR UPDATE SKIP LOCKED` lets multiple workers claim separate jobs safely. The API process does not run encodes. Worker concurrency is configurable and defaults to one; change it only after load testing. Retries are bounded, and stale worker leases are recovered on worker startup/loop. Local disk is scratch space; Cloudinary holds source and result files across API/worker restarts.

Build from repository root with Docker so both `backend/` and `engine/` are available. The CMake target is `compressor`; its container path is `/usr/local/bin/compressor`. See [../DEPLOYMENT.md](../DEPLOYMENT.md) for variables, Docker/Compose commands, health checks, and future EC2 guidance. No AWS deployment is performed by this project setup.

Apply database changes with `npm run migrate:prod` after building, or with the compiled migration command in the container. Do not edit compiled `dist/` output by hand.
