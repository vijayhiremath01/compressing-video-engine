import "dotenv/config";

function getEnv(name: string): string {
    const value = process.env[name];

    if (!value) {
        throw new Error(`Missing required environment variable: ${name}`);
    }

    return value;
}

function getPort(): number {
    const port = Number(process.env.PORT ?? 3000);

    if (!Number.isInteger(port) || port <= 0 || port > 65535) {
        throw new Error(`Invalid PORT value: ${process.env.PORT}`);
    }

    return port;
}

function getCompressorPath(): string {
    const path = process.env.COMPRESSOR_PATH ?? "/usr/local/bin/compressor";
    return path;
}

export const env = {
    nodeEnv: process.env.NODE_ENV ?? "development",
    port: getPort(),
    databaseUrl: getEnv("DATABASE_URL"),
    compressorPath: getCompressorPath(),
    compressionConcurrency: positiveInteger("COMPRESSION_CONCURRENCY", 1),
    compressionMaxAttempts: positiveInteger("COMPRESSION_MAX_ATTEMPTS", 3),
    compressionStaleMs: positiveInteger("COMPRESSION_STALE_MS", 30 * 60 * 1000),
    compressionTimeoutMs: positiveInteger("COMPRESSION_TIMEOUT_MS", 30 * 60 * 1000),
    maxUploadSizeBytes: positiveInteger("MAX_UPLOAD_SIZE_BYTES", 500 * 1024 * 1024),
    rateLimitWindowMs: positiveInteger("RATE_LIMIT_WINDOW_MS", 60_000),
    rateLimitMax: positiveInteger("RATE_LIMIT_MAX", 300),
    compressionRateLimitWindowMs: positiveInteger("COMPRESSION_RATE_LIMIT_WINDOW_MS", 60_000),
    compressionRateLimitMax: positiveInteger("COMPRESSION_RATE_LIMIT_MAX", 5),
    trustProxy: process.env.TRUST_PROXY ?? "false",

    cloudinary: {
        cloudName: getEnv("CLOUDINARY_CLOUD_NAME"),
        apiKey: getEnv("CLOUDINARY_API_KEY"),
        apiSecret: getEnv("CLOUDINARY_API_SECRET"),
    },
};

function positiveInteger(name: string, fallback: number): number {
    const parsed = Number(process.env[name] ?? fallback);
    if (!Number.isSafeInteger(parsed) || parsed <= 0) {
        throw new Error(`Invalid ${name}; expected a positive integer`);
    }
    return parsed;
}
