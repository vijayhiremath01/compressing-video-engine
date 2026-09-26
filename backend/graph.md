## Video Compressor MVP1 — Backend Architecture

```text
                ┌──────────────┐
                │    Client    │
                └──────┬───────┘
                       │
                POST /compression/jobs
                       │
                       ▼
                ┌──────────────┐
                │   Express    │
                │   + Multer   │
                └──────┬───────┘
                       │
                 create UUID
                       │
          ┌────────────┴────────────┐
          ▼                         ▼
    PostgreSQL                  Cloudinary
      QUEUED                    original.mp4
          │                         │
          └────────────┬────────────┘
                       ▼
                ┌──────────────┐
                │ C++ Engine   │
                │   FFmpeg     │
                └──────┬───────┘
                       │
                  output.mp4
                       │
                       ▼
                  Cloudinary
                compressed.mp4
                       │
                       ▼
                  PostgreSQL
                   COMPLETED
                       │
                       ▼
              compressed_url
                       │
                       ▼
                     User