New-Alias which get-command
New-Alias ll dir

rm alias:curl

function l {
  Get-ChildItem $args -Exclude .* | Format-Wide Name -AutoSize
}
Set-Alias ls l -Option AllScope

function g {
  &"$env:Programfiles\Git\cmd\git.exe" "status"
}

function rmrf {
  Param(
    [Parameter(Mandatory=$true)]
    [string]$Target
  )
  Remove-Item -Recurse -Force $Target
}

# Display a character for the provided Unicode value.
# Useful for e.g. checking font installation.
function U
{
  param
  (
    [int] $Code
  )

  if ((0 -le $Code) -and ($Code -le 0xFFFF))
  {
    return [char] $Code
  }

  if ((0x10000 -le $Code) -and ($Code -le 0x10FFFF))
  {
    return [char]::ConvertFromUtf32($Code)
  }

  throw "Invalid character code $Code"
}

Import-Module PowerLine
Import-Module posh-git
Import-Module oh-my-posh
Set-Theme MyTheme
