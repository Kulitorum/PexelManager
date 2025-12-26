# Pexel Manager

Qt-based desktop application for downloading videos and images from Pexels, scaling them, and uploading to AWS S3.

## Build

```bash
cmake -B build
cmake --build build --config Release
```

Or open in Qt Creator and build from there.

## S3 Structure

All media uploads to a single S3 bucket with this structure:
```
s3://decent-de1-media/
├── categories.json      # All categories list
├── catalogs/
│   ├── espresso.json    # Per-category catalog
│   └── landscapes.json
└── media/
    ├── *.mp4            # Scaled video files
    └── *.jpg            # Scaled image files
```

Base URL: `https://decent-de1-media.s3.eu-north-1.amazonaws.com`

## Key Files

- `src/mainwindow.cpp` - Main UI and workflow logic
- `src/uploadmanager.cpp` - Handles scaling (ffmpeg) and S3 uploads (aws cli)
- `src/downloadmanager.cpp` - Downloads media from Pexels
- `src/projectmanager.cpp` - Project file management
- `src/mediametadata.h` - Media data structure (videos and images)
- `src/medialistwidget.cpp` - Media list UI component
- `src/settings.cpp` - Application settings (API keys, AWS profile, media settings)

## Configuration

Settings stored in INI format at `AppData/Roaming/PexelManager/PexelManager/`:
- `api/pexels_key` - Pexels API key (or use `PEXELS_API_KEY` env var)
- `aws/profile` - AWS CLI profile (default: "default")
- `aws/bucket` - S3 bucket for all media (default: "decent-de1-media")

## JSON Formats

### categories.json
```json
[
  {"id": "espresso", "name": "Espresso"},
  {"id": "landscapes", "name": "Landscapes"}
]
```

### catalogs/{categoryId}.json
```json
[
  {
    "id": 1234567,
    "type": "video",
    "path": "1234567_Author_30s.mp4",
    "duration_s": 30,
    "author": "Author Name",
    "bytes": 12345678
  },
  {
    "id": 2345678,
    "type": "image",
    "path": "2345678_Author.jpg",
    "duration_s": 0,
    "author": "Author Name",
    "bytes": 234567
  }
]
```

## Features

- Search Pexels for videos and images by keyword
- Preview media before adding to project
- Download, scale (1280x800), and upload in parallel
- Auto-save project after operations complete
- Upload per-category catalogs for client apps to discover media
- Delete projects from both local storage and S3
- Migration support for old project formats
