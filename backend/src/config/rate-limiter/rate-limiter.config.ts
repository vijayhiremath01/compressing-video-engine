import { rateLimit } from "express-rate-limit";
import { env } from "../env";

function buildLimiter(windowMs: number, limit: number, message: string) {
    return rateLimit({
        windowMs,
        limit,
        standardHeaders: true,
        legacyHeaders: false,
        handler: (_req, res) => res.status(429).json({ message }),
    });
}

export const apiLimiter = buildLimiter(
    env.rateLimitWindowMs,
    env.rateLimitMax,
    "Too many requests. Please try again later."
);

export const compressionLimiter = buildLimiter(
    env.compressionRateLimitWindowMs,
    env.compressionRateLimitMax,
    "Too many compression requests. Please try again later."
);
