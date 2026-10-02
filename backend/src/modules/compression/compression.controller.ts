import { Request, Response, NextFunction } from "express";
import { createJob, getJob } from "./compression.service";
import { uploadVideoToCloudinary } from "./compression.storage";
import { unlink } from "fs/promises";
import { isUUID } from "../../utils/uuid";

export async function createCompressionJob(
    req: Request,
    res: Response,
    _next: NextFunction
): Promise<void> {
    try {
        if (!req.file) {
            res.status(400).json({
                message: "Video file is required",
            });
            return;
        }

        const source = await uploadVideoToCloudinary(req.file.path, "compression/originals", "orig");
        const job = await createJob(req.file.originalname, req.file.size, source.secureUrl, source.publicId);
        console.log(`[${job.id}] Job queued with durable original video`);
        await unlink(req.file.path).catch(() => undefined);

        res.status(202).json({
            message: "Compression job created",
            jobId: job.id,
            status: job.status,
        });
    } catch (error) {
        console.error("Failed to create compression job:", error);

        if (error instanceof Error && error.message.includes("required")) {
            res.status(400).json({
                message: error.message,
            });
            return;
        }

        res.status(500).json({
            message: "Failed to create compression job",
        });
    } finally {
        if (req.file?.path) await unlink(req.file.path).catch(() => undefined);
    }
}

export async function getCompressionJob(
    req: Request,
    res: Response,
    _next: NextFunction
): Promise<void> {
    try {
        const { jobId } = req.params;

        if (!isUUID(jobId)) {
            res.status(400).json({
                message: "Job ID is required",
            });
            return;
        }

        const job = await getJob(jobId);

        if (!job) {
            res.status(404).json({
                message: "Compression job not found",
            });
            return;
        }

        // Return appropriate response based on status
        const response: Record<string, unknown> = {
            jobId: job.id,
            status: job.status,
        };

        if (job.status === "COMPLETED") {
            response.originalFilename = job.originalFilename;
            response.originalSizeBytes = job.originalSizeBytes;
            response.compressedSizeBytes = job.compressedSizeBytes;
            response.compressionRatio = job.compressionRatio;
            response.originalUrl = job.originalUrl;
            response.compressedUrl = job.compressedUrl;
            response.createdAt = job.createdAt;
            response.startedAt = job.startedAt;
            response.completedAt = job.completedAt;
        } else if (job.status === "FAILED") {
            response.errorMessage = job.errorMessage;
        }

        res.status(200).json(response);
    } catch (error) {
        console.error("Failed to get compression job:", error);

        res.status(500).json({
            message: "Failed to get compression job",
        });
    }
}
