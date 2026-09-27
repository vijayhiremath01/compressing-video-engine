import { Router } from "express";
import { upload } from "./config/multer-config/multer.config";
import {
    createCompressionJob,
    getCompressionJob,
} from "./modules/compression/compression.controller";
import { compressionLimiter } from "./config/rate-limiter/rate-limiter.config";


const router = Router();

router.post(
    "/compression/jobs",
    compressionLimiter,
    upload.single("video"),
    createCompressionJob
);

router.get(
    "/compression/jobs/:jobId",
    getCompressionJob
);

export default router;