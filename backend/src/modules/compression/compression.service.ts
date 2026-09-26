import { randomUUID } from "crypto";
import { mkdir, cp, rm, stat } from "fs/promises";
import path from "path";

import {
    createCompressionJob,
    getCompressionJobById,
    updateJobOriginalUrl,
    markJobProcessing,
    markJobCompleted,
    markJobFailed,
} from "./compression.repository";

import {
    CompressionJob,
} from "./compression.type";

import { runCompressionEngine } from "./compression.engine";
import { uploadVideoToCloudinary } from "./compression.storage";

const TEMP_BASE_DIR = "/tmp/compression-jobs";

export async function createJob(
    originalFilename: string,
    originalSizeBytes: number
): Promise<CompressionJob> {
    if (!originalFilename || originalFilename.trim() === "") {
        throw new Error("Original filename is required");
    }

    if (typeof originalSizeBytes !== "number" || originalSizeBytes <= 0) {
        throw new Error("Original size must be a positive number");
    }

    const job: CompressionJob = {
        id: randomUUID(),
        status: "QUEUED",
        originalFilename,
        originalSizeBytes,
        compressedSizeBytes: null,
        compressionRatio: null,
        originalUrl: null,
        compressedUrl: null,
        errorMessage: null,
        createdAt: new Date(),
        startedAt: null,
        completedAt: null,
    };

    await createCompressionJob(job);

    return job;
}

export async function getJob(
    jobId: string
): Promise<CompressionJob | null> {
    if (!jobId || jobId.trim() === "") {
        return null;
    }

    return getCompressionJobById(jobId);
}

async function createJobTempDir(jobId: string): Promise<string> {
    const jobDir = path.join(TEMP_BASE_DIR, jobId);
    await mkdir(jobDir, { recursive: true });
    return jobDir;
}

async function cleanupJobTempDir(jobId: string): Promise<void> {
    const jobDir = path.join(TEMP_BASE_DIR, jobId);
    try {
        await rm(jobDir, { recursive: true, force: true });
    } catch (error) {
        console.warn(`[${jobId}] Failed to cleanup temp dir: ${error instanceof Error ? error.message : "Unknown error"}`);
    }
}

export async function processCompressionJob(
    jobId: string,
    uploadedFilePath: string,
    originalFilename: string,
    originalSizeBytes: number
): Promise<void> {
    const jobDir = await createJobTempDir(jobId);
    const inputPath = path.join(jobDir, "input.mp4");
    const outputPath = path.join(jobDir, "output.mp4");

    console.log(`[${jobId}] Job created`);

    try {
        // Copy uploaded file to job-specific directory
        await cp(uploadedFilePath, inputPath);
        console.log(`[${jobId}] Input file copied to temp dir`);

        // Upload original video to Cloudinary
        console.log(`[${jobId}] Uploading original to Cloudinary...`);
        const originalUpload = await uploadVideoToCloudinary(
            inputPath,
            "compression/originals",
            `orig-${jobId}`
        );
        console.log(`[${jobId}] Original uploaded: ${originalUpload.secureUrl}`);

        // Save original_url
        await updateJobOriginalUrl(jobId, originalUpload.secureUrl);

        // Mark job as PROCESSING
        await markJobProcessing(jobId);
        console.log(`[${jobId}] Compression started`);

        // Run C++ compressor
        const result = await runCompressionEngine(inputPath, outputPath);
        console.log(`[${jobId}] Compression completed. Output size: ${result.outputSizeBytes} bytes`);

        // Upload compressed video to Cloudinary
        console.log(`[${jobId}] Uploading compressed to Cloudinary...`);
        const compressedUpload = await uploadVideoToCloudinary(
            outputPath,
            "compression/compressed",
            `comp-${jobId}`
        );
        console.log(`[${jobId}] Compressed uploaded: ${compressedUpload.secureUrl}`);

        // Calculate compression ratio
        const compressionRatio = ((originalSizeBytes - result.outputSizeBytes) / originalSizeBytes) * 100;

        // Mark job as COMPLETED
        await markJobCompleted(
            jobId,
            result.outputSizeBytes,
            Math.round(compressionRatio * 100) / 100, // Round to 2 decimal places
            compressedUpload.secureUrl
        );
        console.log(`[${jobId}] Job completed. Reduction: ${compressionRatio.toFixed(2)}%`);

    } catch (error) {
        const errorMessage = error instanceof Error ? error.message : "Unknown error";
        console.error(`[${jobId}] Compression failed: ${errorMessage}`);
        await markJobFailed(jobId, errorMessage);
        throw error;
    } finally {
        await cleanupJobTempDir(jobId);
        console.log(`[${jobId}] Cleanup completed`);
    }
}