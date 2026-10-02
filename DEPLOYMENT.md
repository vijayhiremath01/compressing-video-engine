# Future deployment guide

This guide prepares the project for Docker and a later EC2 deployment. No cloud resources are created by these commands.

## Architecture

The API accepts a video, uploads the original to Cloudinary, writes a `QUEUED` row in PostgreSQL, and returns `202` with the job ID. A separate worker claims rows using PostgreSQL `FOR UPDATE SKIP LOCKED`, downloads the durable original, runs the C++ compressor, uploads the result, and updates the row. PostgreSQL is the queue and job source of truth. Worker concurrency defaults to one because encoding is CPU intensive. Workers can later be replicated while retaining safe row claiming.

Local files under `/tmp` are temporary scratch space. Cloudinary stores durable video assets. Failed attempts are retried up to `COMPRESSION_MAX_ATTEMPTS`; expired processing leases are recovered using `COMPRESSION_STALE_MS`.

## Configuration

Copy `backend/.env.example` to `backend/.env` and supply private values locally or through the deployment platform's secret manager:

- `DATABASE_URL`, `CLOUDINARY_CLOUD_NAME`, `CLOUDINARY_API_KEY`, `CLOUDINARY_API_SECRET`
- `PORT`, `NODE_ENV`, `TRUST_PROXY`
- `COMPRESSION_CONCURRENCY`, `COMPRESSION_MAX_ATTEMPTS`, `COMPRESSION_STALE_MS`, `COMPRESSION_TIMEOUT_MS`
- `MAX_UPLOAD_SIZE_BYTES`, `RATE_LIMIT_WINDOW_MS`, `RATE_LIMIT_MAX`, `COMPRESSION_RATE_LIMIT_WINDOW_MS`, `COMPRESSION_RATE_LIMIT_MAX`
- `COMPRESSOR_PATH` defaults to `/usr/local/bin/compressor` in the image; CMake's actual target is `compressor`.

`TRUST_PROXY` defaults to `false`. Set it only to the known number of proxy hops in front of the API (1–10).

## Build and run locally

From the repository root:

```sh
docker compose build
docker compose config
docker compose run --rm api node dist/db/migrate.js
docker compose up -d
```

The API listens on port 3000 in the container and binds to `0.0.0.0`. It provides `/health/live` and `/health/ready`. Readiness checks PostgreSQL and Cloudinary. The worker has no public port. Keep API and worker environment values identical.

For direct Node development, install backend dependencies once, run `npm run build`, then `npm run migrate:prod`, `npm start`, and in another process `npm run worker` from `backend/`. The SQL files are copied into `dist/db/migrations` by the Docker build; for a local TypeScript build, copy them there before running the compiled migration command.

## Later EC2 deployment

Install Docker and Compose on an EC2 host, deliver this repository, add secrets through a protected environment file or secret manager, then build and run the same API and worker services. Allow inbound traffic only to the API port through a load balancer or reverse proxy. Keep PostgreSQL and Cloudinary credentials server-side. Configure the proxy hop count accurately. Do not expose the worker port or the database publicly.

Before public testing, configure backups and retention for PostgreSQL and Cloudinary, TLS at the ingress, monitoring/log shipping, and an operational process for failed jobs. Do not infer user capacity from this architecture: measure upload, database, network, and worker performance under representative load first.

## Load test plan

Measure API request throughput and 429 behavior separately from compression. Then ramp concurrent uploads using small representative MP4 files, monitor queued-job age and database connections, measure worker CPU/RAM and completion time across resolutions/durations, and observe Cloudinary upload latency. Increase `COMPRESSION_CONCURRENCY` only after testing on the target instance size. Include worker restarts during processing to verify stale lease recovery and retries.
