import { spawn } from "child_process";
import { stat } from "fs/promises";
import { env } from "../../config/env";

export interface CompressionEngineResult {
    outputPath: string;
    outputSizeBytes: number;
}

export async function runCompressionEngine(
    inputPath: string,
    outputPath: string,
    timeoutMs = 30 * 60 * 1000
): Promise<CompressionEngineResult> {
    const compressorPath = env.compressorPath;

    return new Promise((resolve, reject) => {
        const compressor = spawn(
            compressorPath,
            [inputPath, outputPath],
            {
                stdio: ["ignore", "pipe", "pipe"],
            }
        );

        const timeout = setTimeout(() => compressor.kill("SIGKILL"), timeoutMs);

        let stdout = "";
        let stderr = "";

        compressor.stdout.on("data", (data: Buffer) => {
            stdout += data.toString();
        });

        compressor.stderr.on("data", (data: Buffer) => {
            stderr += data.toString();
        });

        compressor.on("error", (error) => {
            clearTimeout(timeout);
            reject(
                new Error(
                    `Failed to start compression engine: ${error.message}`
                )
            );
        });

        compressor.on("close", async (code) => {
            clearTimeout(timeout);
            if (code !== 0) {
                reject(
                    new Error(
                        `Compression engine failed with exit code ${code}\n` +
                        `stdout: ${stdout}\n` +
                        `stderr: ${stderr}`
                    )
                );
                return;
            }

            try {
                const outputStats = await stat(outputPath);

                if (outputStats.size <= 0) {
                    reject(
                        new Error(
                            "Compression engine completed but produced an empty output file"
                        )
                    );
                    return;
                }

                resolve({
                    outputPath,
                    outputSizeBytes: outputStats.size,
                });
            } catch (error) {
                reject(
                    new Error(
                        `Compressed output file was not found: ${
                            error instanceof Error
                                ? error.message
                                : "Unknown error"
                        }`
                    )
                );
            }
        });
    });
}
