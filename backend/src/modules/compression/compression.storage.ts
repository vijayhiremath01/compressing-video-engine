import { v2 as cloudinary } from "cloudinary";
import { env } from "../../config/env";
import { Readable } from "stream";
import { createReadStream } from "fs";

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
                unsigned: true,
                upload_preset: "ml_default",
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
                unsigned: true,
                upload_preset: "ml_default",
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

        const readable = new Readable();
        readable.push(buffer);
        readable.push(null);
        readable.pipe(uploadStream);
    });
}