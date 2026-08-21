$ErrorActionPreference = 'Stop'

$repositoryRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..\..')

Describe 'Android libc++ portability includes' {
    It 'declares the standard header used by SohMenuRandomizer string streams' {
        $sourcePath = Join-Path $repositoryRoot 'soh\soh\SohGui\SohMenuRandomizer.cpp'
        $source = Get-Content -LiteralPath $sourcePath -Raw

        $source | Should Match '(?m)^#include <sstream>$'
    }
}
