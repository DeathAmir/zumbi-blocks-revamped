$ErrorActionPreference = "Stop"
$repo = "DeathAmir/zumbi-blocks-revamped"
$exe = Join-Path $PSScriptRoot "..\build\Release\zumbi_native.exe"
$keyFile = Join-Path $PSScriptRoot "..\NativeEngine\keys\asset_signing_public_key.hex"
if (!(Test-Path $exe)) {
    throw "Build NativeEngine first. Expected: $exe"
}
$privateDirectory = Join-Path $env:USERPROFILE ".zbr"
New-Item -ItemType Directory -Force -Path $privateDirectory | Out-Null
$privatePath = Join-Path $privateDirectory "asset-signing-private.hex"
if (Test-Path $privatePath) {
    $privateKey = (Get-Content -Path $privatePath -Raw).Trim()
    if ($privateKey -notmatch '^[0-9a-fA-F]{64}$') {
        throw "Existing private signing key is invalid; refusing to rotate it automatically."
    }
    $privateKey = $privateKey.ToLowerInvariant()
} else {
    $rng = [System.Security.Cryptography.RandomNumberGenerator]::Create()
    $bytes = New-Object byte[] 32
    $rng.GetBytes($bytes)
    $rng.Dispose()
    $privateKey = [Convert]::ToHexString($bytes).ToLowerInvariant()
    Set-Content -Path $privatePath -Value $privateKey -NoNewline -Encoding ascii
}
icacls $privatePath /inheritance:r /grant:r "$($env:USERNAME):(R,W)" | Out-Null
$env:ZBR_ASSET_SIGNING_PRIVATE_KEY = $privateKey
$publicKey = (& $exe asset-public-key-env | Out-String).Trim()
if ($LASTEXITCODE -ne 0 -or $publicKey -notmatch '^[0-9a-f]{64}$') {
    throw "Could not derive the Ed25519 public key."
}
Set-Content -Path $keyFile -Value $publicKey -NoNewline -Encoding ascii
if (Get-Command gh -ErrorAction SilentlyContinue) {
    $privateKey | gh secret set ZBR_ASSET_SIGNING_PRIVATE_KEY --repo $repo
    if ($LASTEXITCODE -ne 0) {
        throw "GitHub CLI could not create/update the Actions secret. Check gh auth status and repository access."
    }
    Write-Host "GitHub Actions secret ZBR_ASSET_SIGNING_PRIVATE_KEY was created or updated."
} else {
    Write-Warning "GitHub CLI is not installed. Install/authenticate gh, then run:"
    Write-Host "Get-Content $env:USERPROFILE\.zbr\asset-signing-private.hex -Raw | gh secret set ZBR_ASSET_SIGNING_PRIVATE_KEY --repo $repo"
    Write-Host "Or add the private key from the local file at https://github.com/$repo/settings/secrets/actions/new"
}
Write-Host "Public key written to NativeEngine/keys/asset_signing_public_key.hex"
Write-Host "Private key is stored outside the repository at $privatePath"
Write-Host "The existing private key is reused on later runs; it is never printed or committed."
