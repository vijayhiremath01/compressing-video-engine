import multer from "multer";
import path from "path";

const ALLOWED_VIDEO_MIME_TYPES = [
    "video/mp4",
    "video/mpeg",
    "video/quicktime",
    "video/x-msvideo",
    "video/x-matroska",
    "video/webm",
    "application/mp4",
    "application/octet-stream",
];

const MAX_FILE_SIZE = 500 * 1024 * 1024;

const fileFilter = (
    _req: Express.Request,
    file: Express.Multer.File,
    cb: multer.FileFilterCallback
): void => {
    if (!ALLOWED_VIDEO_MIME_TYPES.includes(file.mimetype)) {
        cb(new Error(`Invalid file type. Allowed types: ${ALLOWED_VIDEO_MIME_TYPES.join(", ")}`));
        return;
    }
    cb(null, true);
};

const storage = multer.diskStorage({
    destination: (_req, _file, cb) => {
        cb(null, path.join(process.cwd(), "uploads"));
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