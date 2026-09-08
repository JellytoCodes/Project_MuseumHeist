Set-StrictMode -Version Latest

function Get-PackageDataSha256 {
	param([Parameter(Mandatory = $true)]$Value)

	$json = ConvertTo-Json -InputObject $Value -Depth 8 -Compress
	$sha256 = [System.Security.Cryptography.SHA256]::Create()
	try {
		$hash = $sha256.ComputeHash([System.Text.Encoding]::UTF8.GetBytes($json))
		return ([System.BitConverter]::ToString($hash)).Replace('-', '').ToLowerInvariant()
	}
	finally {
		$sha256.Dispose()
	}
}

function Get-PackageSourceSnapshot {
	param(
		[Parameter(Mandatory = $true)][string]$ProjectRoot,
		[string]$OutputPath = ''
	)

	$pathspec = @('--', '.')
	$root = [System.IO.Path]::GetFullPath($ProjectRoot).TrimEnd('\', '/')
	if ($OutputPath -and $OutputPath.StartsWith($root + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase)) {
		$pathspec += ':(exclude,literal)' + $OutputPath.Substring($root.Length + 1).Replace('\', '/')
	}
	$commit = & git -C $ProjectRoot rev-parse HEAD 2>$null
	if ($LASTEXITCODE -ne 0 -or [string]$commit -notmatch '^(?:[0-9a-f]{40}|[0-9a-f]{64})$') {
		throw 'Cannot capture the packaging Git revision.'
	}
	$statusOutput = & git -C $ProjectRoot status --porcelain=v1 --untracked-files=all -z @pathspec 2>$null
	if ($LASTEXITCODE -ne 0) {
		throw 'Cannot capture the packaging Git working-tree state.'
	}
	$status = $statusOutput -join "`n"
	$pathsOutput = & git -C $ProjectRoot -c core.quotepath=false ls-files --cached --others --exclude-standard -z @pathspec 2>$null
	if ($LASTEXITCODE -ne 0) {
		throw 'Cannot enumerate packaging source inputs.'
	}
	[string[]]$paths = @(($pathsOutput -join "`n").Split([char]0, [System.StringSplitOptions]::RemoveEmptyEntries) | Select-Object -Unique)
	[Array]::Sort($paths, [System.StringComparer]::Ordinal)
	$files = foreach ($relativePath in $paths) {
		$path = Join-Path $ProjectRoot $relativePath
		if (Test-Path -LiteralPath $path -PathType Leaf) {
			$file = Get-Item -LiteralPath $path -Force
			[pscustomobject][ordered]@{ path = $relativePath; sizeBytes = $file.Length; sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() }
		}
		elseif (Test-Path -LiteralPath $path) {
			throw "Packaging input is not a regular file: $relativePath"
		}
		else {
			[pscustomobject][ordered]@{ path = $relativePath; sizeBytes = -1; sha256 = 'missing' }
		}
	}
	[pscustomobject]@{
		GitCommitFull = [string]$commit
		GitCommit = ([string]$commit).Substring(0, 12)
		GitDirty = $status.Length -gt 0
		InputSha256 = Get-PackageDataSha256 -Value ([ordered]@{ gitCommit = [string]$commit; status = $status; files = @($files) })
	}
}

function Get-PackageExternalContentSnapshot {
	param([Parameter(Mandatory = $true)][string]$ProjectRoot)

	$root = [System.IO.Path]::GetFullPath($ProjectRoot).TrimEnd('\', '/')
	$scopeRoot = 'Content/Assets/MapAssets'
	$directory = Join-Path $root $scopeRoot
	if (-not (Test-Path -LiteralPath $directory -PathType Container)) {
		throw "External content directory is missing: $directory"
	}
	[string[]]$paths = @(Get-ChildItem -LiteralPath $directory -Recurse -File -Force | ForEach-Object { $_.FullName.Substring($root.Length + 1).Replace('\', '/') })
	[Array]::Sort($paths, [System.StringComparer]::Ordinal)
	$files = foreach ($relativePath in $paths) {
		$path = Join-Path $root $relativePath
		$file = Get-Item -LiteralPath $path -Force
		[pscustomobject][ordered]@{ path = $relativePath; sizeBytes = $file.Length; sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() }
	}
	[pscustomobject][ordered]@{ schemaVersion = 1; scopeRoot = $scopeRoot; files = @($files) }
}

function Assert-PackageExternalContent {
	param(
		[Parameter(Mandatory = $true)][string]$ProjectRoot,
		[Parameter(Mandatory = $true)][string]$LockPath
	)

	$lock = Get-Content -LiteralPath $LockPath -Raw | ConvertFrom-Json
	if ($lock.schemaVersion -ne 1 -or $lock.scopeRoot -ne 'Content/Assets/MapAssets' -or @($lock.files).Count -eq 0) {
		throw 'External content lock has an unsupported schema or empty scope.'
	}
	$expected = [System.Collections.Generic.Dictionary[string, object]]::new([System.StringComparer]::Ordinal)
	foreach ($entry in $lock.files) {
		if ($entry.path -notmatch '^Content/Assets/MapAssets/[^\\]+$' -or $entry.path -match '(^|/)\.\.?(/|$)' -or
			$entry.sizeBytes -lt 0 -or $entry.sha256 -notmatch '^[0-9a-f]{64}$' -or $expected.ContainsKey($entry.path)) {
			throw "External content lock contains an invalid or duplicate entry: $($entry.path)"
		}
		$expected.Add($entry.path, $entry)
	}
	$actual = Get-PackageExternalContentSnapshot -ProjectRoot $ProjectRoot
	foreach ($entry in $actual.files) {
		if (-not $expected.ContainsKey($entry.path)) {
			throw "External content is not locked: $($entry.path)"
		}
		$locked = $expected[$entry.path]
		if ($entry.sizeBytes -ne $locked.sizeBytes -or $entry.sha256 -ne $locked.sha256) {
			throw "External content differs from the lock: $($entry.path)"
		}
		$expected.Remove($entry.path) | Out-Null
	}
	if ($expected.Count -ne 0) {
		throw "Locked external content is missing: $(@($expected.Keys)[0])"
	}
	return (Get-FileHash -LiteralPath $LockPath -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Assert-PackageInputsUnchanged {
	param(
		[Parameter(Mandatory = $true)]$Before,
		[Parameter(Mandatory = $true)]$After,
		[Parameter(Mandatory = $true)][string]$ExternalLockBefore,
		[Parameter(Mandatory = $true)][string]$ExternalLockAfter
	)

	if ($Before.GitCommitFull -ne $After.GitCommitFull -or $Before.InputSha256 -ne $After.InputSha256 -or $ExternalLockBefore -ne $ExternalLockAfter) {
		throw 'Packaging inputs changed during RunUAT. The output must not be published as the captured revision.'
	}
}
