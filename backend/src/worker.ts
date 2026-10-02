import os from "os";
import { pool } from "./config/psql-db-config/pool.config";
import { env } from "./config/env";
import { claimJob, processClaimedJob, recoverJobs } from "./modules/compression/compression.service";

const workerId = `${os.hostname()}-${process.pid}`;
let stopping = false;

async function runWorker(): Promise<void> {
    console.log(`Compression worker ${workerId} starting with concurrency ${env.compressionConcurrency}`);
    const active = new Set<Promise<void>>();
    while (!stopping) {
        await recoverJobs();
        while (!stopping && active.size < env.compressionConcurrency) {
            const job = await claimJob(workerId);
            if (!job) break;
            console.log(`[${job.id}] Job claimed by worker ${workerId} (attempt ${job.attempts})`);
            let task!: Promise<void>;
            task = processClaimedJob(job).finally(() => active.delete(task));
            active.add(task);
        }
        if (active.size === 0) await new Promise((resolve) => setTimeout(resolve, 1500));
        else await Promise.race([Promise.race(active), new Promise((resolve) => setTimeout(resolve, 500))]);
    }
    await Promise.allSettled(active);
    await pool.end();
}

for (const signal of ["SIGINT", "SIGTERM"] as const) {
    process.on(signal, () => { stopping = true; });
}

runWorker().catch(async (error) => {
    console.error("Worker stopped unexpectedly:", error instanceof Error ? error.message : "Unknown error");
    await pool.end();
    process.exitCode = 1;
});
