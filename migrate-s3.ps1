# migrate-s3.ps1
# Migrates data from old multi-bucket structure to new single-bucket structure

$NEW_BUCKET = "decent-de1-media"
$CATEGORIES = @("espresso", "landscapes", "birthday", "newyear")

# 1. Copy media files from each old bucket
foreach ($cat in $CATEGORIES) {
    Write-Host "Copying videos from decent-de1-$cat..." -ForegroundColor Cyan
    aws s3 sync "s3://decent-de1-$cat/videos/" "s3://$NEW_BUCKET/media/" --exclude "catalog.json"
}

# 2. Transform and upload catalogs
foreach ($cat in $CATEGORIES) {
    Write-Host "Transforming catalog for $cat..." -ForegroundColor Cyan

    # Download old catalog
    $tempOld = "$env:TEMP\old-$cat.json"
    $tempNew = "$env:TEMP\$cat.json"

    aws s3 cp "s3://decent-de1-$cat/videos/catalog.json" $tempOld

    # Add "type": "video" to each entry
    $catalog = Get-Content $tempOld | ConvertFrom-Json
    $newCatalog = $catalog | ForEach-Object {
        $_ | Add-Member -NotePropertyName "type" -NotePropertyValue "video" -PassThru
    }
    $newCatalog | ConvertTo-Json -Depth 10 | Set-Content $tempNew -Encoding UTF8

    # Upload to new location
    aws s3 cp $tempNew "s3://$NEW_BUCKET/catalogs/$cat.json"
}

# 3. Create new categories.json (without bucket field)
Write-Host "Creating categories.json..." -ForegroundColor Cyan
$tempOldCat = "$env:TEMP\old-categories.json"
$tempNewCat = "$env:TEMP\categories.json"

aws s3 cp "s3://decent-de1-categories/categories.json" $tempOldCat

$categories = Get-Content $tempOldCat | ConvertFrom-Json
$newCategories = $categories | ForEach-Object {
    [PSCustomObject]@{ id = $_.id; name = $_.name }
}
$newCategories | ConvertTo-Json -Depth 10 | Set-Content $tempNewCat -Encoding UTF8

aws s3 cp $tempNewCat "s3://$NEW_BUCKET/categories.json"

Write-Host "Done! Verify at: https://$NEW_BUCKET.s3.eu-north-1.amazonaws.com/categories.json" -ForegroundColor Green
