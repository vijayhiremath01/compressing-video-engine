import express from "express";
import cors from "cors";
import path from "path";
import multer from "multer";
import routes from "./routes";

const app = express();

app.use(cors({
  origin: true,
  credentials: true,
}));
app.use(express.json());

app.use("/uploads", express.static(path.join(process.cwd(), "uploads")));

app.use("/api", routes);

app.get("/health", (_req, res) => {
    res.status(200).json({
        status: "ok",
    });
});

app.use((err: Error, _req: express.Request, res: express.Response, _next: express.NextFunction) => {
    console.error("Unhandled error:", err);

    if (err instanceof multer.MulterError) {
        if (err.code === "LIMIT_FILE_SIZE") {
            return res.status(413).json({
                message: "File too large. Maximum size is 500MB.",
            });
        }
        return res.status(400).json({
            message: err.message,
        });
    }

    if (err.message.includes("Invalid file type")) {
        return res.status(400).json({
            message: err.message,
        });
    }

    res.status(500).json({
        message: "Internal server error",
    });
});

export default app;