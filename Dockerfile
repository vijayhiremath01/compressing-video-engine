FROM debian:bookworm-slim AS engine-build
RUN apt-get update && apt-get install -y --no-install-recommends build-essential cmake pkg-config libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libswresample-dev libx264-dev && rm -rf /var/lib/apt/lists/*
WORKDIR /src/engine
COPY engine/CMakeLists.txt ./
COPY engine/src ./src
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --target compressor -j2 && test -x build/compressor

FROM node:22-bookworm-slim AS backend-build
WORKDIR /app
COPY backend/package*.json ./
RUN npm ci
COPY backend/tsconfig.json ./
COPY backend/src ./src
RUN npm run build && mkdir -p dist/db/migrations && cp src/db/migrations/*.sql dist/db/migrations/

FROM node:22-bookworm-slim AS production-deps
WORKDIR /app
COPY backend/package*.json ./
RUN npm ci --omit=dev

FROM node:22-bookworm-slim AS runtime
ENV NODE_ENV=production PORT=3000 COMPRESSOR_PATH=/usr/local/bin/compressor
RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates libavcodec59 libavformat59 libavutil57 libswscale6 libswresample4 libx264-164 && rm -rf /var/lib/apt/lists/* && groupadd --system app && useradd --system --gid app --home-dir /app app
WORKDIR /app
COPY --from=backend-build --chown=app:app /app/dist ./dist
COPY --from=backend-build --chown=app:app /app/package.json ./package.json
COPY --from=production-deps --chown=app:app /app/node_modules ./node_modules
COPY --from=engine-build --chown=app:app /src/engine/build/compressor /usr/local/bin/compressor
RUN test -x "$COMPRESSOR_PATH" && mkdir -p /tmp/compression-jobs /tmp/compression-uploads && chown -R app:app /tmp/compression-jobs /tmp/compression-uploads
USER app
EXPOSE 3000
CMD ["node", "dist/server.js"]
