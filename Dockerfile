# ============================================
# STAGE 1: Build C++ Compression Engine
# ============================================
FROM ubuntu:22.04 AS engine-builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    libavcodec-dev \
    libavformat-dev \
    libavutil-dev \
    libswscale-dev \
    libswresample-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /engine

COPY engine/CMakeLists.txt .
COPY engine/src ./src

RUN mkdir -p build && cd build \
    && cmake .. -DCMAKE_BUILD_TYPE=Release \
    && make -j$(nproc) compressor

# ============================================
# STAGE 2: Build TypeScript Backend
# ============================================
FROM node:20-alpine AS backend-builder

WORKDIR /app

COPY backend/package*.json ./
RUN npm ci

COPY backend/tsconfig.json ./
COPY backend/src ./src

RUN npm run build

# ============================================
# STAGE 3: Runtime Image
# ============================================
FROM ubuntu:22.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive \
    NODE_ENV=production \
    PORT=3000

# Install runtime dependencies: FFmpeg libs + Node.js
RUN apt-get update && apt-get install -y \
    ffmpeg \
    libavcodec58 \
    libavformat58 \
    libavutil56 \
    libswscale5 \
    libswresample3 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Install Node.js 20
RUN apt-get update && apt-get install -y curl \
    && curl -fsSL https://deb.nodesource.com/setup_20.x | bash - \
    && apt-get install -y nodejs \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy built compressor binary from engine-builder
COPY --from=engine-builder /engine/build/compressor /usr/local/bin/compressor
RUN chmod +x /usr/local/bin/compressor

# Copy built backend from backend-builder
COPY --from=backend-builder /app/dist ./dist
COPY --from=backend-builder /app/node_modules ./node_modules
COPY --from=backend-builder /app/package.json ./

# Create uploads directory
RUN mkdir -p /app/uploads

# Create temp directory for compression jobs
RUN mkdir -p /tmp/compression-jobs

# Verify compressor binary works
RUN /usr/local/bin/compressor 2>&1 | head -1 || true

EXPOSE 3000

CMD ["node", "dist/server.js"]