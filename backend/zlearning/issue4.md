# Issue 4: Missing Global Error Handler for Multer Errors

## Problem
When multer encounters errors (file too large, invalid file type, etc.), they were not being caught properly, resulting in:
- Unhandled promise rejections
- Generic 500 errors instead of specific 400/413 errors
- No user-friendly error messages

### Files Affected
- `src/app.ts` - Missing error handling middleware

### Before (Buggy)
```typescript
// app.ts - No error handler
import express from "express";

const app = express();
app.use(express.json());

app.get("/health", (_req, res) => {
    res.status(200).json({ status: "ok" });
});

export default app;
```

If a user uploads a 600MB file, multer throws `MulterError: File too large` but there's no handler for it.

### After (Fixed)
```typescript
// app.ts - With global error handler
import express from "express";
import path from "path";
import multer from "multer";
import routes from "./routes";

const app = express();
app.use(express.json());
app.use("/uploads", express.static(path.join(process.cwd(), "uploads")));
app.use("/api", routes);

app.get("/health", (_req, res) => {
    res.status(200).json({ status: "ok" });
});

// ✅ Global error handler for multer and other errors
app.use((err: Error, _req: express.Request, res: express.Response, _next: express.NextFunction) => {
    console.error("Unhandled error:", err);

    // Handle multer-specific errors
    if (err instanceof multer.MulterError) {
        if (err.code === "LIMIT_FILE_SIZE") {
            return res.status(413).json({
                message: "File too large. Maximum size is 500MB.",
            });
        }
        if (err.code === "LIMIT_FILE_COUNT") {
            return res.status(400).json({
                message: "Too many files. Only one video file allowed.",
            });
        }
        if (err.code === "LIMIT_UNEXPECTED_FILE") {
            return res.status(400).json({
                message: "Unexpected file field. Use 'video' as field name.",
            });
        }
        return res.status(400).json({
            message: err.message,
        });
    }

    // Handle custom file type errors from fileFilter
    if (err.message.includes("Invalid file type")) {
        return res.status(400).json({
            message: err.message,
        });
    }

    // Generic error fallback
    res.status(500).json({
        message: "Internal server error",
    });
});

export default app;
```

## Root Cause
Express error handlers must be:
1. Registered **after** all routes
2. Have **4 parameters** (`err, req, res, next`)
3. Specifically check for multer error types

Multer errors are instances of `MulterError` with a `code` property.

## Common Multer Error Codes

| Code | HTTP Status | Description |
|------|-------------|-------------|
| `LIMIT_FILE_SIZE` | 413 | File exceeds `limits.fileSize` |
| `LIMIT_FILE_COUNT` | 400 | Too many files |
| `LIMIT_UNEXPECTED_FILE` | 400 | File field name doesn't match |
| `LIMIT_PART_COUNT` | 400 | Too many parts in multipart form |
| `LIMIT_FIELD_KEY` | 400 | Field name too long |
| `LIMIT_FIELD_VALUE` | 400 | Field value too long |
| `LIMIT_FIELD_COUNT` | 400 | Too many fields |

## Solution Approach
1. Import `multer` in app.ts (needed for `instanceof` check)
2. Add error handler middleware **after** route registration
3. Check for `MulterError` first, then custom errors
4. Return appropriate HTTP status codes

## Verification
```bash
# Test file too large (create a 600MB test file)
dd if=/dev/zero of=large.mp4 bs=1M count=600
curl -X POST -F "video=@large.mp4" http://localhost:3000/api/compression/jobs
# Should return 413: "File too large. Maximum size is 500MB."

# Test invalid file type
echo "not a video" > test.txt
curl -X POST -F "video=@test.txt" http://localhost:3000/api/compression/jobs
# Should return 400: "Invalid file type. Allowed types: video/mp4, video/mpeg, ..."

# Test wrong field name
curl -X POST -F "file=@test.mp4" http://localhost:3000/api/compression/jobs
# Should return 400: "Unexpected file field. Use 'video' as field name."
```

## Prevention for Future
- Always add global error handler in Express apps
- Handle multer errors specifically before generic errors
- Log errors for debugging but don't expose stack traces to clients
- Use consistent error response format across the API

## Key Takeaway
**Always add a global error handler that specifically handles multer errors.** Without it, file upload failures result in unhelpful 500 errors instead of clear 400/413 responses.