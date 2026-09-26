# Issue 2: Missing Multer Middleware Configuration for File Uploads

## Problem
The controller uses `req.file` to access uploaded files, but:
1. No multer middleware was configured in the Express app
2. No routes were registered for the compression endpoints
3. File uploads would always result in `req.file` being `undefined`

### Files Affected
- `src/app.ts` - Missing multer config and route registration
- `src/modules/compression/compression.controller.ts` - Assumes `req.file` exists

### Before (Buggy)
```typescript
// app.ts - No multer, no routes
import express from "express";

const app = express();
app.use(express.json());

app.get("/health", (_req, res) => {
    res.status(200).json({ status: "ok" });
});

export default app;

// compression.controller.ts - Assumes req.file exists
if (!req.file) {  // Always true because no multer middleware
    res.status(400).json({ message: "Video file is required!" });
    return;
}
```

### After (Fixed)
```typescript
// src/config/multer-config/multer.config.ts - New file
import multer from "multer";
import path from "path";

const ALLOWED_VIDEO_MIME_TYPES = [
    "video/mp4",
    "video/mpeg",
    "video/quicktime",
    "video/x-msvideo",
    "video/x-matroska",
    "video/webm",
];

const MAX_FILE_SIZE = 500 * 1024 * 1024; // 500MB

const fileFilter = (_req, file, cb) => {
    if (!ALLOWED_VIDEO_MIME_TYPES.includes(file.mimetype)) {
        cb(new Error(`Invalid file type. Allowed: ${ALLOWED_VIDEO_MIME_TYPES.join(", ")}`));
        return;
    }
    cb(null, true);
};

const storage = multer.diskStorage({
    destination: (_req, _file, cb) => {
        cb(null, path.join(process.cwd(), "uploads"));
    },
    filename: (_req, file, cb) => {
        const uniqueSuffix = `${Date.now()}-${Math.round(Math.random() * 1e9)}`;
        const ext = path.extname(file.originalname);
        cb(null, `${file.fieldname}-${uniqueSuffix}${ext}`);
    },
});

export const upload = multer({
    storage,
    fileFilter,
    limits: { fileSize: MAX_FILE_SIZE },
});

// src/routes.ts - New file
import { Router } from "express";
import { upload } from "./config/multer-config/multer.config";
import { createCompressionJob, getCompressionJob } from "./modules/compression/compression.controller";

const router = Router();

router.post("/compression/jobs", upload.single("video"), createCompressionJob);
router.get("/compression/jobs/:jobId", getCompressionJob);

export default router;

// app.ts - Updated
import express from "express";
import path from "path";
import multer from "multer";
import routes from "./routes";

const app = express();
app.use(express.json());
app.use("/uploads", express.static(path.join(process.cwd(), "uploads")));
app.use("/api", routes);  // Register routes

app.get("/health", (_req, res) => {
    res.status(200).json({ status: "ok" });
});

// Global error handler for multer errors
app.use((err, _req, res, _next) => {
    if (err instanceof multer.MulterError) {
        if (err.code === "LIMIT_FILE_SIZE") {
            return res.status(413).json({ message: "File too large. Maximum size is 500MB." });
        }
        return res.status(400).json({ message: err.message });
    }
    if (err.message.includes("Invalid file type")) {
        return res.status(400).json({ message: err.message });
    }
    res.status(500).json({ message: "Internal server error" });
});

export default app;
```

## Root Cause
Express doesn't handle multipart/form-data by default. You need:
1. **multer** middleware to parse multipart form data
2. **Storage configuration** (disk or memory)
3. **File filter** for validation
4. **Route registration** to connect endpoints

## Solution Approach
1. Create multer configuration with:
   - Disk storage (saves to `uploads/` folder)
   - File type validation (video MIME types only)
   - File size limit (500MB)
   - Unique filename generation
2. Create routes file to register endpoints
3. Update app.ts to use routes and add global error handler
4. Create uploads directory

## Verification
```bash
npm run build  # Should compile
# Test with curl:
curl -X POST -F "video=@test.mp4" http://localhost:3000/api/compression/jobs
```

## Prevention for Future
- Always configure multer before using `req.file` or `req.files`
- Validate file type AND size in multer config (not just in controller)
- Use global error handler for multer-specific errors (LIMIT_FILE_SIZE, etc.)
- Store uploads outside of source code (use `process.cwd()` or config)

## Key Takeaway
**Never assume `req.file` exists without configuring multer middleware first.** The middleware must be applied to the route (or globally) before the controller runs.