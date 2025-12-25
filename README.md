# Pexel Manager

A Qt 6 desktop application for searching, downloading, scaling, and uploading Pexels stock videos to AWS S3.

## Features

- **Search Pexels**: Search the Pexels video library with filters for minimum duration and maximum resolution
- **Video Preview**: Preview videos with adjustable playback speed (0.5x - 10x)
- **Project Management**: Organize videos into projects, each with its own S3 bucket
- **Batch Processing**: Download, scale, and upload multiple videos with progress tracking
- **Smart Filtering**: Automatically filters out already-approved or rejected videos from search results

## Requirements

- Qt 6.x (with Multimedia and Network modules)
- CMake 3.16+
- FFmpeg (for video scaling)
- AWS CLI (for S3 uploads)

## Building

```bash
cmake -B build -DCMAKE_PREFIX_PATH=/path/to/qt
cmake --build build
```

## Usage

1. **Configure Settings** (File → Settings):
   - Enter your Pexels API key
   - Set AWS profile and default S3 bucket
   - Configure target resolution and FFmpeg settings

2. **Create/Open Project** (File → New/Open Project):
   - Each project has its own S3 bucket and video collection

3. **Search & Review**:
   - Search for videos using keywords
   - Use arrow keys to navigate and preview videos
   - Press DEL to reject videos (supports multi-select with Ctrl/Shift)
   - Click "Add to Project" to approve remaining videos

4. **Process Videos**:
   - **Download Selected**: Downloads videos at chosen resolution
   - **Scale Downloaded**: Scales videos using FFmpeg
   - **Upload to S3**: Uploads scaled videos to the project's S3 bucket

## License

MIT
