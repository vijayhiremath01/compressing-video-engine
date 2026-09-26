# ZLearning - Bug Fixes Documentation

This folder contains documentation for bugs fixed in the video compression backend.

## Issues Fixed

| Issue | File | Description |
|-------|------|-------------|
| [Issue 1](issue1.md) | `compression.type.ts`, `repository.ts`, `service.ts` | Using `String` object wrapper instead of `string` primitive type |
| [Issue 2](issue2.md) | `app.ts`, new `multer.config.ts`, new `routes.ts` | Missing multer middleware configuration for file uploads |
| [Issue 3](issue3.md) | `controller.ts`, `service.ts`, `repository.ts` | Missing input validation at all layers |
| [Issue 4](issue4.md) | `app.ts` | Missing global error handler for multer errors |

## Quick Reference: Common Patterns

### ✅ Correct: Primitive Types
```typescript
id: string;
name: string;
count: number;
flag: boolean;
```

### ❌ Wrong: Object Wrappers
```typescript
id: String;    // Don't use
name: String;  // Don't use
count: Number; // Don't use
flag: Boolean; // Don't use
```

### ✅ Correct: Defense in Depth Validation
```typescript
// Controller - HTTP layer
if (!jobId) { return res.status(400).json(...); }

// Service - Business logic layer
if (!filename) { throw new Error("Filename required"); }

// Repository - Data layer
if (!jobId) { return null; }
```

### ✅ Correct: Multer Configuration
```typescript
const upload = multer({
    storage: multer.diskStorage({...}),
    fileFilter: (req, file, cb) => { /* validate type */ },
    limits: { fileSize: 500 * 1024 * 1024 }
});

// Apply to route
router.post("/jobs", upload.single("video"), controller);
```

### ✅ Correct: Global Error Handler
```typescript
app.use((err, req, res, next) => {
    if (err instanceof multer.MulterError) {
        if (err.code === "LIMIT_FILE_SIZE") {
            return res.status(413).json({ message: "File too large" });
        }
    }
    if (err.message.includes("Invalid file type")) {
        return res.status(400).json({ message: err.message });
    }
    res.status(500).json({ message: "Internal server error" });
});
```

## How to Use This for Future Bugs

When you encounter similar issues:

1. **Identify the layer** - Controller, Service, Repository, Config?
2. **Check the pattern** - Look at the corresponding issue file
3. **Apply the fix** - Use the "After" code as template
4. **Verify** - Run `npm run build` and test with curl
5. **Document** - Add new issue file if it's a new pattern

## Commands

```bash
# Build check
npm run build

# Run server
npm run dev

# Test endpoints
curl -X POST -F "video=@test.mp4" http://localhost:3000/api/compression/jobs
curl http://localhost:3000/api/compression/jobs/<job-id>
```