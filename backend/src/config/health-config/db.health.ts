import { pool } from "../psql-db-config/pool.config";

export async function checkDbConnection(): Promise<void> {
    const client = await pool.connect();

    try {
        await client.query("SELECT 1");
        console.log("PostgreSQL connected successfully");
    } finally {
        client.release();
    }
}