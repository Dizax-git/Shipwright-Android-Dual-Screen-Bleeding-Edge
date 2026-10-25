$ErrorActionPreference = 'Stop'
$repositoryRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..\..')
Describe 'Stable 9.3.1 extraction failure behavior on Android' {
    It 'tracks first-ROM success only after Torch succeeds on the worker thread' {
        $source = Get-Content (Join-Path $repositoryRoot 'soh/soh/OTRGlobals.cpp') -Raw
        $androidFirst = [regex]::Match($source, '(?s)case PS_FIRST: \{\s*#ifdef __ANDROID__(.*?)#else').Groups[1].Value
        $androidFirst | Should Match 'extractionTask = threadPool->submit_task'
        $androidFirst | Should Match 'extractFailed\s*=\s*!extract.CallTorch'
        $androidFirst | Should Match '(?s)if \(!extractFailed\).*?generatedFirst = true;'
    }
}
