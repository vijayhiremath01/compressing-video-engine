import { pool } from "../../config/psql-db-config/pool.config";
import {
    CompressionJob,
    CompressionStatus,
} from "./compression.type";

export async function createCompressionJob(
    job: CompressionJob
): Promise<void> {
    await pool.query(
        `
        INSERT INTO compression_jobs (
            id,
            status,
            original_filename,
            original_size_bytes,
            compressed_size_bytes,
            compression_ratio,
            original_url,
            compressed_url,
            error_message,
            created_at,
            started_at,
            completed_at
        )
        VALUES (
            $1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12
        )
        `,
        [
            job.id,
            job.status,
            job.originalFilename,
            job.originalSizeBytes,
            job.compressedSizeBytes,
            job.compressionRatio,
            job.originalUrl,
            job.compressedUrl,
            job.errorMessage,
            job.createdAt,
            job.startedAt,
            job.completedAt,
        ]
    );
}

export async function getCompressionJobById(
    jobId: string
): Promise<CompressionJob | null> {
    if (!jobId || jobId.trim() === "") {
        return null;
    }

    const result = await pool.query(
        `
        SELECT
            id,
            status,
            original_filename,
            original_size_bytes,
            compressed_size_bytes,
            compression_ratio,
            original_url,
            compressed_url,
            error_message,
            created_at,
            started_at,
            completed_at
        FROM compression_jobs
        WHERE id = $1
        `,
        [jobId]
    );

    if (result.rows.length === 0) {
        return null;
    }

    const row = result.rows[0];

    return {
        id: row.id,
        status: row.status as CompressionStatus,

        originalFilename: row.original_filename,
        originalSizeBytes: Number(row.original_size_bytes),

        compressedSizeBytes:
            row.compressed_size_bytes === null
                ? null
                : Number(row.compressed_size_bytes),

        compressionRatio:
            row.compression_ratio === null
                ? null
                : Number(row.compression_ratio),

        originalUrl: row.original_url,
        compressedUrl: row.compressed_url,

        errorMessage: row.error_message,

        createdAt: row.created_at,
        startedAt: row.started_at,
        completedAt: row.completed_at,
    };
}

export async function updateJobOriginalUrl(
    jobId: string,
    originalUrl: string
): Promise<void> {
    await pool.query(
        `
        UPDATE compression_jobs
        SET original_url = $1
        WHERE id = $2
        `,
        [originalUrl, jobId]
    );
}

export async function markJobProcessing(
    jobId: string
): Promise<void> {
    await pool.query(
        `
        UPDATE compression_jobs
        SET status = 'PROCESSING', started_at = NOW()
        WHERE id = $1
        `,
        [jobId]
    );
}

export async function markJobCompleted(
    jobId: string,
    compressedSizeBytes: number,
    compressionRatio: number,
    compressedUrl: string
): Promise<void> {
    await pool.query(
        `
        UPDATE compression_jobs
        SET
            status = 'COMPLETED',
            compressed_size_bytes = $1,
            compression_ratio = $2,
            compressed_url = $3,
            completed_at = NOW()
        WHERE id = $4
        `,
        [compressedSizeBytes, compressionRatio, compressedUrl, jobId]
    );
}

export async function markJobFailed(
    jobId: string,
    errorMessage: string
): Promise<void> {
    await pool.query(
        `
        UPDATE compression_jobs
        SET
            status = 'FAILED',
            error_message = $1,
            completed_at = NOW()
        WHERE id = $2
        `,
        [errorMessage, jobId]
    );
}