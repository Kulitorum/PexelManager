# Pexel Manager

Qt-based desktop application for downloading videos from Pexels, scaling them, and uploading to AWS S3.

## Build

```bash
cmake -B build
cmake --build build --config Release
```

Or open in Qt Creator and build from there.

## S3 Structure

Each project uploads to its own S3 bucket with this structure:
```
s3://decent-de1-{category}/
├── index.json           # Project metadata
└── videos/
    ├── catalog.json     # List of all videos with metadata
    └── *.mp4            # Scaled video files
```

A central categories manifest is uploaded to:
```
s3://decent-de1-categories/
└── categories.json      # Lists all available category buckets
```

## Key Files

- `src/mainwindow.cpp` - Main UI and workflow logic
- `src/uploadmanager.cpp` - Handles scaling (ffmpeg) and S3 uploads (aws cli)
- `src/downloadmanager.cpp` - Downloads videos from Pexels
- `src/projectmanager.cpp` - Project file management
- `src/settings.cpp` - Application settings (API keys, AWS profile, video settings)

## Configuration

Settings stored in INI format at `AppData/Roaming/PexelManager/PexelManager/`:
- `api/pexels_key` - Pexels API key (or use `PEXELS_API_KEY` env var)
- `aws/profile` - AWS CLI profile (default: "default")
- `aws/bucket` - Default S3 bucket for new projects
- `aws/categories_bucket` - Bucket for categories.json (default: "decent-de1-categories")

## JSON Formats

### categories.json
```json
[
  {"id": "espresso", "name": "Espresso", "bucket": "decent-de1-espresso"},
  {"id": "landscapes", "name": "Landscapes", "bucket": "decent-de1-landscapes"}
]
```

### catalog.json
```json
[
  {
    "id": 1234567,
    "path": "1234567_Author_30s.mp4",
    "duration_s": 30,
    "author": "Author Name",
    "bytes": 12345678
  }
]
```

## Features

- Search Pexels for videos by keyword
- Preview videos before adding to project
- Download, scale (1280x800), and upload in parallel
- Auto-save project after operations complete
- Upload catalog.json for client apps to discover videos
- Delete projects from both local storage and S3
