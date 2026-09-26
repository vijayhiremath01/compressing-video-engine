import { rateLimit } from "express-rate-limit";

export const compressionLimiter = rateLimit({
    windowMs: 60 * 1000, // 1 minute
    limit: 10,
    standardHeaders: "draft-8",
    legacyHeaders: false,
    ipv6Subnet: 56,

    message: {
        message: "Too many compression requests. Please try again later."
    }
});