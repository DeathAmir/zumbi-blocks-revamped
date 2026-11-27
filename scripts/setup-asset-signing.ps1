$ErrorActionPreference = "Stop"
$repo = "DeathAmir/zumbi-blocks-revamped"
$exe = Join-Path $PSScriptRoot "..\build\Release\zumbi_native.exe"
$keyFile = Join-Path $PSScriptRoot "..\NativeEngine\keys\asset_signing_public_key.hex"
if (!(Test-Path $exe)) {
    throw "Build NativeEngine first. Expected: $exe"
}
$rng = [System.Security.Cryptography.RandomNumberGenerator]::Create()
$bytes = New-Object byte[] 32
$rng.GetBytes($bytes)
$rng.Dispose()
$privateKey = [Convert]::ToHexString($bytes).ToLowerInvariant()
$env:ZBR_ASSET_SIGNING_PRIVATE_KEY = $privateKey
$publicKey = (& $exe asset-public-key-env | Out-String).Trim()
if ($LASTEXITCODE -ne 0 -or $publicKey -notmatch '^[0-9a-f]{64}$') {
    throw "Could not derive the Ed25519 public key."
}
Set-Content -Path $keyFile -Value $publicKey -NoNewline -Encoding ascii
$privateDirectory = Join-Path $env:USERPROFILE ".zbr"
New-Item -ItemType Directory -Force -Path $privateDirectory | Out-Null
$privatePath = Join-Path $privateDirectory "asset-signing-private.hex"
Set-Content -Path $privatePath -Value $privateKey -NoNewline -Encoding ascii
icacls $privatePath /inheritance:r /grant:r "$($env:USERNAME):(R,W)" | Out-Null
if (Get-Command gh -ErrorAction SilentlyContinue) {
    gh secret set ZBR_ASSET_SIGNING_PRIVATE_KEY --repo $repo --body $privateKey
    if ($LASTEXITCODE -eq 0) {
        Write-Host "GitHub Actions secret ZBR_ASSET_SIGNING_PRIVATE_KEY was created."
    } else {
        Set-Clipboard -Value $privateKey
        Write-Warning "GitHub CLI could not set the secret. The private key is in your clipboard."
        Write-Host "Add it at https://github.com/$repo/settings/secrets/actions/new"
    }
} else {
    Set-Clipboard -Value $privateKey
    Write-Warning "GitHub CLI is not installed. The private key is in your clipboard."
    Write-Host "Add secret ZBR_ASSET_SIGNING_PRIVATE_KEY at https://github.com/$repo/settings/secrets/actions/new"
}
Write-Host "Public key written to NativeEngine/keys/asset_signing_public_key.hex"
Write-Host "Private key is stored outside the repository at $privatePath"
Write-Host "Never commit or upload the private key."
