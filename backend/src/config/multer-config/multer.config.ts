import multer from "multer";
import path from "path";
import { mkdirSync } from "fs";

const ALLOWED_VIDEO_MIME_TYPES = [
    "video/mp4",
    "video/mpeg",
    "video/quicktime",
    "video/x-msvideo",
    "video/x-matroska",
    "video/webm",
    "video/3gpp",
    "video/3gpp2",
    "application/mp4",
    "application/octet-stream",
    "binary/octet-stream",
    "video/*",
];

import { env } from "../env";

const MAX_FILE_SIZE = env.maxUploadSizeBytes;

const fileFilter = (
    _req: Express.Request,
    file: Express.Multer.File,
    cb: multer.FileFilterCallback
): void => {
    const isVideo = ALLOWED_VIDEO_MIME_TYPES.includes(file.mimetype);
    if (!isVideo) {
        cb(new Error(`Invalid file type: ${file.mimetype}. Allowed: video/*`));
        return;
    }
    cb(null, true);
};

const storage = multer.diskStorage({
    destination: (_req, _file, cb) => {
        const destination = "/tmp/compression-uploads";
        mkdirSync(destination, { recursive: true });
        cb(null, destination);
    },
    filename: (_req, file, cb) => {
        const uniqueSuffix = `${Date.now()}-${Math.round(Math.random() * 1e9)}`;
        const ext = path.extname(file.originalname);
        cb(null, `${file.fieldname}-${uniqueSuffix}${ext}`);
    },
});

export const upload = multer({
    storage,
    fileFilter,
    limits: {
        fileSize: MAX_FILE_SIZE,
    },
});

export const ALLOWED_VIDEO_TYPES = ALLOWED_VIDEO_MIME_TYPES;
export const MAX_UPLOAD_SIZE = MAX_FILE_SIZE;
