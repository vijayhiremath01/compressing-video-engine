import { Pool } from "pg";
import { env } from "../env";

function parseConnectionString(url: string) {
    const parsed = new URL(url);
    return {
        host: parsed.hostname,
        port: parseInt(parsed.port || "5432"),
        database: parsed.pathname.slice(1),
        user: parsed.username,
        password: parsed.password,
        ssl: parsed.searchParams.get("sslmode") === "disable" ? false : { rejectUnauthorized: false },
    };
}

const config = parseConnectionString(env.databaseUrl);

export const pool = new Pool({
    ...config,
    max: 10,
    idleTimeoutMillis: 30_000,
    connectionTimeoutMillis: 30_000,
});

pool.on("error", (error) => {
    console.error("Unexpected PostgreSQL pool error:", error);
});

pool.on("error", (error) => {
    console.error("Unexpected PostgreSQL pool error:", error);
});