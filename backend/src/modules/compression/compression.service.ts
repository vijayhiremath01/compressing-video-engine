import { randomUUID } from "crypto";
import { mkdir, rm, stat } from "fs/promises";
import { createWriteStream } from "fs";
import { Readable } from "stream";
import { pipeline } from "stream/promises";
import path from "path";
import { env } from "../../config/env";
import { claimCompressionJob, createCompressionJob, getCompressionJobById, markJobCompleted, recoverStaleJobs, retryOrFailJob } from "./compression.repository";
import { CompressionJob } from "./compression.type";
import { runCompressionEngine } from "./compression.engine";
import { uploadVideoToCloudinary } from "./compression.storage";

export async function createJob(filename: string, size: number, originalUrl: string, publicId: string): Promise<CompressionJob> {
    const job: CompressionJob = {
        id: randomUUID(), status: "QUEUED", originalFilename: filename, originalSizeBytes: size,
        compressedSizeBytes: null, compressionRatio: null, originalUrl, originalPublicId: publicId,
        compressedUrl: null, errorMessage: null, createdAt: new Date(), startedAt: null, completedAt: null,
        attempts: 0, maxAttempts: env.compressionMaxAttempts,
    };
    await createCompressionJob(job);
    return job;
}

async function downloadOriginal(url: string, destination: string): Promise<void> {
    const response = await fetch(url);
    if (!response.ok || !response.body) throw new Error(`Could not retrieve original video (HTTP ${response.status})`);
    await pipeline(Readable.fromWeb(response.body as import("stream/web").ReadableStream), createWriteStream(destination));
}

export async function processClaimedJob(job: CompressionJob): Promise<void> {
    const dir = path.join("/tmp/compression-jobs", job.id);
    const input = path.join(dir, "input.mp4");
    const output = path.join(dir, "output.mp4");
    try {
        await mkdir(dir, { recursive: true });
        if (!job.originalUrl) throw new Error("Original video URL is missing");
        await downloadOriginal(job.originalUrl, input);
        const originalStat = await stat(input);
        if (originalStat.size === 0) throw new Error("Downloaded original video is empty");
        const result = await runCompressionEngine(input, output, env.compressionTimeoutMs);
        const uploaded = await uploadVideoToCloudinary(output, "compression/compressed", `comp-${job.id}`);
        const reduction = ((job.originalSizeBytes - result.outputSizeBytes) / job.originalSizeBytes) * 100;
        await markJobCompleted(job.id, result.outputSizeBytes, Math.round(reduction * 100) / 100, uploaded.secureUrl);
        console.log(`[${job.id}] Job completed`);
    } catch (error) {
        const message = error instanceof Error ? error.message : "Unknown compression failure";
        await retryOrFailJob(job, message);
        console.error(`[${job.id}] Job attempt ${job.attempts} failed: ${message}`);
    } finally {
        await rm(dir, { recursive: true, force: true });
        console.log(`[${job.id}] Cleanup completed`);
    }
}

export async function claimJob(workerId: string): Promise<CompressionJob | null> {
    return claimCompressionJob(workerId);
}

export async function getJob(jobId: string): Promise<CompressionJob | null> {
    return getCompressionJobById(jobId);
}

export async function recoverJobs(): Promise<void> {
    await recoverStaleJobs(env.compressionStaleMs);
}
