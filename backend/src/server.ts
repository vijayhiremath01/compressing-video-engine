import app from "./app";
import { env } from "./config/env";
import { checkCloudinary } from "./config/health-config/cloudinary.health";
import { checkDbConnection } from "./config/health-config/db.health";
import { rateLimit } from "express-rate-limit" ;


async function startServer() : Promise<void> {
    try{
        await checkDbConnection();
        await checkCloudinary();

        app.listen(env.port, () => {
            console.log(`Server running on port ${env.port}`);
        });
    } catch(error){
        console.error("Failed to start server:", error);
        process.exit(1);
    }
}

startServer();