import express from "express";
import multer from "multer";
import { env } from "./config/env";
import { checkDbConnection } from "./config/health-config/db.health";
import { checkCloudinary } from "./config/health-config/cloudinary.health";
import { apiLimiter } from "./config/rate-limiter/rate-limiter.config";
import routes from "./routes";

const app = express();
if (env.trustProxy !== "false") {
    const hops = Number(env.trustProxy);
    if (!Number.isSafeInteger(hops) || hops < 1 || hops > 10) throw new Error("TRUST_PROXY must be false or an integer hop count from 1 to 10");
    app.set("trust proxy", hops);
}

app.disable("x-powered-by");
app.use((_req, res, next) => {
    res.setHeader("X-Content-Type-Options", "nosniff");
    res.setHeader("X-Frame-Options", "DENY");
    res.setHeader("Referrer-Policy", "no-referrer");
    next();
});
app.use(express.json({ limit: "64kb" }));
app.use(apiLimiter);
app.use("/api", routes);

app.get("/health", (_req, res) => res.status(200).json({ status: "ok" }));
app.get("/health/live", (_req, res) => res.status(200).json({ status: "ok" }));
app.get("/health/ready", async (_req, res) => {
    try {
        await checkDbConnection();
        await checkCloudinary();
        res.status(200).json({ status: "ready" });
    } catch {
        res.status(503).json({ status: "not_ready" });
    }
});

app.use((err: Error, _req: express.Request, res: express.Response, _next: express.NextFunction) => {
    if (err instanceof multer.MulterError) {
        const tooLarge = err.code === "LIMIT_FILE_SIZE";
        return res.status(tooLarge ? 413 : 400).json({
            message: tooLarge ? "File too large. Maximum size is 500MB." : err.code === "LIMIT_UNEXPECTED_FILE" ? "Unexpected upload field. Use the video field." : err.message,
        });
    }
    if (/Invalid file type/.test(err.message)) return res.status(400).json({ message: "Unsupported video file type." });
    if (/multipart|boundary|form end/i.test(err.message)) return res.status(400).json({ message: "Malformed multipart upload." });
    console.error("Request failed:", err.message);
    return res.status(500).json({ message: "Internal server error" });
});

export default app;
