import { randomUUID } from "crypto";
import { pool } from "../../config/psql-db-config/pool.config";
import { CompressionJob, CompressionStatus } from "./compression.type";

export async function createCompressionJob(job: CompressionJob): Promise<void> {
    await pool.query(
        `INSERT INTO compression_jobs (id,status,original_filename,original_size_bytes,original_url,original_public_id,created_at,queued_at,max_attempts)
         VALUES ($1,'QUEUED',$2,$3,$4,$5,NOW(),NOW(),$6)`,
        [job.id, job.originalFilename, job.originalSizeBytes, job.originalUrl, job.originalPublicId, job.maxAttempts]
    );
}

export async function getCompressionJobById(jobId: string): Promise<CompressionJob | null> {
    const result = await pool.query(`SELECT * FROM compression_jobs WHERE id = $1`, [jobId]);
    if (!result.rows[0]) return null;
    const r = result.rows[0];
    return {
        id: r.id, status: r.status as CompressionStatus,
        originalFilename: r.original_filename, originalSizeBytes: Number(r.original_size_bytes),
        compressedSizeBytes: r.compressed_size_bytes == null ? null : Number(r.compressed_size_bytes),
        compressionRatio: r.compression_ratio == null ? null : Number(r.compression_ratio),
        originalUrl: r.original_url, originalPublicId: r.original_public_id,
        compressedUrl: r.compressed_url, errorMessage: r.error_message,
        createdAt: r.created_at, startedAt: r.started_at, completedAt: r.completed_at,
        attempts: r.attempts, maxAttempts: r.max_attempts,
    };
}

export async function claimCompressionJob(workerId: string): Promise<CompressionJob | null> {
    const client = await pool.connect();
    try {
        await client.query("BEGIN");
        const result = await client.query(
            `SELECT * FROM compression_jobs WHERE status='QUEUED' AND attempts < max_attempts
             ORDER BY queued_at ASC FOR UPDATE SKIP LOCKED LIMIT 1`
        );
        if (!result.rows[0]) { await client.query("COMMIT"); return null; }
        const r = result.rows[0];
        await client.query(
            `UPDATE compression_jobs SET status='PROCESSING', attempts=attempts+1, started_at=NOW(), locked_at=NOW(), worker_id=$2 WHERE id=$1`,
            [r.id, workerId]
        );
        await client.query("COMMIT");
        return getCompressionJobById(r.id);
    } catch (error) { await client.query("ROLLBACK"); throw error; }
    finally { client.release(); }
}

export async function recoverStaleJobs(staleMs: number): Promise<void> {
    await pool.query(
        `UPDATE compression_jobs SET
            status=CASE WHEN attempts >= max_attempts THEN 'FAILED' ELSE 'QUEUED' END,
            error_message=CASE WHEN attempts >= max_attempts THEN 'Worker stopped during processing; retry limit reached' ELSE error_message END,
            last_error='Worker lease expired', worker_id=NULL, locked_at=NULL,
            queued_at=NOW(), completed_at=CASE WHEN attempts >= max_attempts THEN NOW() ELSE NULL END
         WHERE status='PROCESSING' AND locked_at < NOW() - ($1 * INTERVAL '1 millisecond')`, [staleMs]
    );
}

export async function markJobCompleted(jobId: string, size: number, ratio: number, url: string): Promise<void> {
    await pool.query(
        `UPDATE compression_jobs SET status='COMPLETED', compressed_size_bytes=$1, compression_ratio=$2, compressed_url=$3,
         completed_at=NOW(), locked_at=NULL, worker_id=NULL, error_message=NULL WHERE id=$4`, [size, ratio, url, jobId]
    );
}

export async function retryOrFailJob(job: CompressionJob, message: string): Promise<void> {
    await pool.query(
        `UPDATE compression_jobs SET status=CASE WHEN attempts < max_attempts THEN 'QUEUED' ELSE 'FAILED' END,
         error_message=CASE WHEN attempts < max_attempts THEN NULL ELSE $2 END, last_error=$2,
         queued_at=NOW(), completed_at=CASE WHEN attempts < max_attempts THEN NULL ELSE NOW() END,
         locked_at=NULL, worker_id=NULL WHERE id=$1`, [job.id, message.slice(0, 4000)]
    );
}
