# ROOT-only host for the SAME reviewed owner. This file never writes registry.
param([Parameter(Mandatory)][string]$Owner,[Parameter(Mandatory)][string]$OwnerSha256,
 [Parameter(Mandatory)][string]$ArgumentsJson,[Parameter(Mandatory)][string]$ArgumentsSha256,
 [Parameter(Mandatory)][ValidateSet('9','9ex','10')][string]$Api,
 [ValidateSet('offscreen','present')][string]$Phase='offscreen',
 [switch]$RootAuthorizeLifecycleRestart)
$ErrorActionPreference='Stop';Set-StrictMode -Version Latest
if(!$RootAuthorizeLifecycleRestart){throw 'Explicit ROOT lifecycle attempt required; default refuses'}
if($Api -ceq '10' -and $Phase -cne 'offscreen'){throw 'API10 requires its existing phase selector; its native probe includes Present'}
foreach($pair in @(@($Owner,$OwnerSha256),@($ArgumentsJson,$ArgumentsSha256))){
 if($pair[1] -cnotmatch '^[0-9a-f]{64}$' -or (Get-FileHash -LiteralPath $pair[0] -Algorithm SHA256).Hash.ToLowerInvariant() -cne $pair[1]){throw 'Reviewed owner/actual arguments hash differs'}
}
$value=ConvertFrom-Json ([IO.File]::ReadAllText($ArgumentsJson))
$keys=@('RunRoot','InstanceId','Luid','PrivateLoader','PrivateLoaderSha256','Probe','ProbeSha256','Front','FrontSha256','Core','CoreSha256','Runner','DriverSys','DriverSysSha256','VulkanIcd','VulkanIcdSha256','VulkanLibrarySha256','VulkanLoaderSha256','ApprovedPayload','ApprovedPayloadSha256','TokenScript','TokenScriptSha256')
if(@($value.PSObject.Properties).Count -ne $keys.Count){throw 'Exact reviewed existing-owner argument set required'}
$params=@{Role='Controller';Api=$Api;D9Phase=$Phase;D11Phase='offscreen';ApplyReviewedTuple=$true;RootAuthorizeLifecycleRestart=$true}
foreach($key in $keys){if($null -eq $value.PSObject.Properties[$key] -or $value.$key -isnot [string] -or !$value.$key){throw 'Actual nonnull existing-owner argument required'};$params[$key]=$value.$key}
$global:LASTEXITCODE=$null
& $Owner @params
if($null -eq $LASTEXITCODE){throw 'Existing owner must publish its actual exit code'}
exit ([int]$LASTEXITCODE)
