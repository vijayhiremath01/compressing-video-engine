import { v2 as cloudinary } from "cloudinary";
import { env } from "../../config/env";

cloudinary.config({
    cloud_name: env.cloudinary.cloudName,
    api_key: env.cloudinary.apiKey,
    api_secret: env.cloudinary.apiSecret,
    secure: true,
});

export interface CloudinaryUploadResult {
    secureUrl: string;
    publicId: string;
    bytes: number;
    format: string;
}

export async function uploadVideoToCloudinary(
    filePath: string,
    folder: string,
    publicIdPrefix: string
): Promise<CloudinaryUploadResult> {
    return new Promise((resolve, reject) => {
        const publicId = `${publicIdPrefix}-${Date.now()}-${Math.random().toString(36).substring(2, 9)}`;

        cloudinary.uploader.upload(
            filePath,
            {
                resource_type: "video",
                folder,
                public_id: publicId,
                overwrite: true,
            },
            (error, result) => {
                if (error) {
                    reject(new Error(`Cloudinary upload failed: ${error.message}`));
                    return;
                }
                if (!result) {
                    reject(new Error("Cloudinary upload returned no result"));
                    return;
                }
                resolve({
                    secureUrl: result.secure_url,
                    publicId: result.public_id,
                    bytes: result.bytes,
                    format: result.format,
                });
            }
        );
    });
}

export async function uploadVideoBufferToCloudinary(
    buffer: Buffer,
    folder: string,
    publicIdPrefix: string
): Promise<CloudinaryUploadResult> {
    return new Promise((resolve, reject) => {
        const publicId = `${publicIdPrefix}-${Date.now()}-${Math.random().toString(36).substring(2, 9)}`;

        const uploadStream = cloudinary.uploader.upload_stream(
            {
                resource_type: "video",
                folder,
                public_id: publicId,
                overwrite: true,
            },
            (error, result) => {
                if (error) {
                    reject(new Error(`Cloudinary upload failed: ${error.message}`));
                    return;
                }
                if (!result) {
                    reject(new Error("Cloudinary upload returned no result"));
                    return;
                }
                resolve({
                    secureUrl: result.secure_url,
                    publicId: result.public_id,
                    bytes: result.bytes,
                    format: result.format,
                });
            }
        );

        const { Readable } = require("stream") as typeof import("stream");
        const readable = Readable.from(buffer);
        readable.pipe(uploadStream);
    });
}
