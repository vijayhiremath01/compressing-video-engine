import { pool } from "../config/psql-db-config/pool.config";
import { readFileSync, readdirSync } from "fs";
import { join } from "path";

async function runMigrations(): Promise<void> {
    const client = await pool.connect();

    try {
        await client.query(`
            CREATE TABLE IF NOT EXISTS schema_migrations (
                version VARCHAR(50) PRIMARY KEY,
                applied_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
            );
        `);

        const migrationFiles = readdirSync(join(__dirname, "migrations"))
            .filter((file) => /^\d+_.*\.sql$/.test(file)).sort();

        for (const file of migrationFiles) {
            const version = file.split("_")[0];

            const { rows } = await client.query(
                "SELECT 1 FROM schema_migrations WHERE version = $1",
                [version]
            );

            if (rows.length > 0) {
                console.log(`Migration ${version} already applied, skipping`);
                continue;
            }

            const sql = readFileSync(join(__dirname, "migrations", file), "utf-8");
            await client.query("BEGIN");
            try {
                await client.query(sql);
                await client.query("INSERT INTO schema_migrations (version) VALUES ($1)", [version]);
                await client.query("COMMIT");
            } catch (error) {
                await client.query("ROLLBACK");
                throw error;
            }
            console.log(`Applied migration ${version}`);
        }

        console.log("All migrations completed successfully");
    } finally {
        client.release();
        await pool.end();
    }
}

runMigrations().catch((err) => {
    console.error("Migration failed:", err);
    process.exit(1);
});
