-- Migration: 001_create_compression_jobs.sql
-- Creates the compression_jobs table for MVP1 video compression

CREATE TABLE IF NOT EXISTS compression_jobs (
    id UUID PRIMARY KEY,
    status VARCHAR(20) NOT NULL CHECK (status IN ('QUEUED', 'PROCESSING', 'COMPLETED', 'FAILED')),
    original_filename VARCHAR(255) NOT NULL,
    original_size_bytes BIGINT NOT NULL,
    compressed_size_bytes BIGINT NULL,
    compression_ratio DECIMAL(10,4) NULL,
    original_url TEXT NULL,
    compressed_url TEXT NULL,
    error_message TEXT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    started_at TIMESTAMPTZ NULL,
    completed_at TIMESTAMPTZ NULL
);

CREATE INDEX IF NOT EXISTS idx_compression_jobs_status ON compression_jobs(status);
CREATE INDEX IF NOT EXISTS idx_compression_jobs_created_at ON compression_jobs(created_at);