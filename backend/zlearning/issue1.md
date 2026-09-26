# Issue 1: Using `String` (Object Wrapper) Instead of `string` (Primitive) Type

## Problem
In TypeScript, `String` (capital S) is the object wrapper type, while `string` (lowercase) is the primitive type. Using `String` throughout the codebase causes:
- Unnecessary object wrapping
- Type mismatches when comparing with primitive strings
- Potential runtime issues with methods like `.trim()` on `String` objects

### Files Affected
- `src/modules/compression/compression.type.ts`
- `src/modules/compression/compression.repository.ts`
- `src/modules/compression/compression.service.ts`

### Before (Buggy)
```typescript
// compression.type.ts
export interface CompressionJob {
    id: String;        // ❌ Object wrapper
    status: CompressionStatus;
    originalFilename: String;  // ❌
    // ...
}

// compression.repository.ts
export async function getCompressionJobById(
    jobId: String  // ❌ Parameter type
): Promise<CompressionJob | null>

// compression.service.ts
export async function getJob(
    jobId: String  // ❌ Parameter type
): Promise<CompressionJob | null>
```

### After (Fixed)
```typescript
// compression.type.ts
export interface CompressionJob {
    id: string;        // ✅ Primitive type
    status: CompressionStatus;
    originalFilename: string;  // ✅
    // ...
}

// compression.repository.ts
export async function getCompressionJobById(
    jobId: string  // ✅ Primitive type
): Promise<CompressionJob | null>

// compression.service.ts
export async function getJob(
    jobId: string  // ✅ Primitive type
): Promise<CompressionJob | null>
```

## Root Cause
TypeScript has two string types:
- `string` - primitive type (what you want 99% of the time)
- `String` - object wrapper type (rarely needed)

The object wrapper `String` has methods like `toString()`, `valueOf()`, but it behaves differently in comparisons and can cause subtle bugs.

## Solution Approach
1. Search for all occurrences of `String` (capital S) used as type annotations
2. Replace with `string` (lowercase)
3. Run TypeScript compiler to verify no type errors

## Verification
```bash
npm run build
# Should compile without errors
```

## Prevention for Future
- Always use `string`, `number`, `boolean`, `symbol`, `bigint` for primitive types
- Only use `String`, `Number`, `Boolean` when you specifically need the object wrapper
- Enable TypeScript strict mode: `"strict": true` in tsconfig.json
- Use ESLint rule `@typescript-eslint/no-wrapper-object-types` to catch this

## Key Takeaway
**Always use primitive types (`string`, `number`, `boolean`) for type annotations in TypeScript.** The object wrappers (`String`, `Number`, `Boolean`) are almost never what you want.