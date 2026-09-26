# Issue 3: Missing Input Validation in Controller and Service

## Problem
The code lacks proper input validation at multiple layers:
1. Controller doesn't validate jobId parameter
2. Service doesn't validate filename or file size
3. Repository doesn't validate jobId before database query

### Files Affected
- `src/modules/compression/compression.controller.ts`
- `src/modules/compression/compression.service.ts`
- `src/modules/compression/compression.repository.ts`

### Before (Buggy)
```typescript
// controller.ts - No validation of jobId
export async function getCompressionJob(req: Request, res: Response): Promise<void> {
    const { jobId } = req.params;  // Could be empty string, undefined
    const job = await getJob(jobId);  // Passes invalid ID to service
    // ...
}

// service.ts - No validation of inputs
export async function createJob(originalFilename: string, originalSizeBytes: number): Promise<CompressionJob> {
    // No check for empty filename
    // No check for negative/zero size
    const job: CompressionJob = { ... };
    await createCompressionJob(job);
    return job;
}

export async function getJob(jobId: String): Promise<CompressionJob | null> {
    // No validation - passes empty string to DB
    return getCompressionJobById(jobId);
}

// repository.ts - No validation before query
export async function getCompressionJobById(jobId: String): Promise<CompressionJob | null> {
    const result = await pool.query("SELECT ... WHERE id = $1", [jobId]);
    // Query runs even with empty/invalid jobId
    // ...
}
```

### After (Fixed)
```typescript
// controller.ts - Validates jobId parameter
export async function getCompressionJob(req: Request, res: Response, _next: NextFunction): Promise<void> {
    try {
        const { jobId } = req.params;

        if (!jobId || jobId.trim() === "") {  // ✅ Validate
            res.status(400).json({ message: "Job ID is required" });
            return;
        }

        const job = await getJob(jobId);
        // ...
    }
}

// service.ts - Validates all inputs
export async function createJob(originalFilename: string, originalSizeBytes: number): Promise<CompressionJob> {
    if (!originalFilename || originalFilename.trim() === "") {  // ✅ Validate
        throw new Error("Original filename is required");
    }

    if (typeof originalSizeBytes !== "number" || originalSizeBytes <= 0) {  // ✅ Validate
        throw new Error("Original size must be a positive number");
    }

    const job: CompressionJob = { ... };
    await createCompressionJob(job);
    return job;
}

export async function getJob(jobId: string): Promise<CompressionJob | null> {
    if (!jobId || jobId.trim() === "") {  // ✅ Validate
        return null;
    }
    return getCompressionJobById(jobId);
}

// repository.ts - Validates before query
export async function getCompressionJobById(jobId: string): Promise<CompressionJob | null> {
    if (!jobId || jobId.trim() === "") {  // ✅ Validate
        return null;
    }
    // Query only runs with valid jobId
    const result = await pool.query("SELECT ... WHERE id = $1", [jobId]);
    // ...
}
```

## Root Cause
Defensive programming was not applied. Each layer assumed the previous layer validated the data, but:
- Controller is the entry point (should validate HTTP params)
- Service is the business logic layer (should validate business rules)
- Repository is the data layer (should validate before DB queries)

## Solution Approach
Apply **defense in depth** - validate at every layer:

| Layer | Responsibility | What to Validate |
|-------|---------------|------------------|
| Controller | HTTP input | Required params, format, types |
| Service | Business rules | Required fields, value ranges, formats |
| Repository | Data integrity | Valid IDs, non-null required fields |

## Validation Patterns

### 1. Early Return Pattern (Controller)
```typescript
if (!jobId || jobId.trim() === "") {
    res.status(400).json({ message: "Job ID is required" });
    return;  // Stop execution early
}
```

### 2. Throw Error Pattern (Service)
```typescript
if (!originalFilename || originalFilename.trim() === "") {
    throw new Error("Original filename is required");
}
```

### 3. Null Return Pattern (Repository)
```typescript
if (!jobId || jobId.trim() === "") {
    return null;  // Don't query DB with invalid ID
}
```

## Verification
```bash
# Test missing file
curl -X POST http://localhost:3000/api/compression/jobs
# Should return 400: "Video file is required"

# Test missing jobId
curl http://localhost:3000/api/compression/jobs/
# Should return 400: "Job ID is required"

# Test invalid jobId
curl http://localhost:3000/api/compression/jobs/invalid-uuid
# Should return 404: "Compression job not found"
```

## Prevention for Future
- Validate at the boundary of each layer
- Use a validation library (zod, joi, class-validator) for complex schemas
- Create reusable validation helpers
- Add integration tests for invalid inputs

## Key Takeaway
**Validate early, validate often.** Every layer should validate its inputs - don't trust that the previous layer did it correctly.