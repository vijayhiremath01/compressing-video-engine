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
    const path = process.env.COMPRESSOR_PATH;
    if (!path) {
        throw new Error("Missing required environment variable: COMPRESSOR_PATH");
    }
    return path;
}

export const env = {
    nodeEnv: process.env.NODE_ENV ?? "development",
    port: getPort(),
    databaseUrl: getEnv("DATABASE_URL"),
    compressorPath: getCompressorPath(),

    cloudinary: {
        cloudName: getEnv("CLOUDINARY_CLOUD_NAME"),
        apiKey: getEnv("CLOUDINARY_API_KEY"),
        apiSecret: getEnv("CLOUDINARY_API_SECRET"),
    },
};