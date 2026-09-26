import cloudinary from "../cloudinary-conifg/cloudinary.config";

export async function checkCloudinary() : Promise<void> {
    try{
        await cloudinary.api.ping(); 
        console.log("Cloudinary connected successfully");
    } catch(error) {
        console.error("Cloudinary connection failed:", error);
        throw error;
    }
}

