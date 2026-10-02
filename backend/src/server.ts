import app from "./app";
import { env } from "./config/env";
import { checkDbConnection } from "./config/health-config/db.health";


async function startServer() : Promise<void> {
    try{
        await checkDbConnection();

        const server = app.listen(env.port, "0.0.0.0", () => {
            console.log(`API server listening on 0.0.0.0:${env.port}`);
        });
        const shutdown = () => server.close(() => process.exit(0));
        process.on("SIGTERM", shutdown);
        process.on("SIGINT", shutdown);
    } catch(error){
        console.error("Failed to start server:", error);
        process.exit(1);
    }
}

startServer();
