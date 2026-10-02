export type CompressionStatus =
    | "QUEUED"
    | "PROCESSING"
    | "COMPLETED"
    | "FAILED";

export interface CompressionJob {
    id: string;
    status: CompressionStatus;

    originalFilename: string;
    originalSizeBytes: number;

    compressedSizeBytes: number | null;
    compressionRatio: number | null;

    originalUrl: string | null;
    originalPublicId: string | null;
    compressedUrl: string | null;

    errorMessage: string | null;

    createdAt: Date;
    startedAt: Date | null;
    completedAt: Date | null;
    attempts: number;
    maxAttempts: number;
}
