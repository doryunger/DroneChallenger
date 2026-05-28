$CenterLat  = 48.1374
$CenterLon  = 11.5755
$RadiusM    = 2000
$OutDir     = "$PSScriptRoot\..\Content\Graph"

$HighwayFilter = "motorway|trunk|primary|secondary|tertiary|unclassified|residential|living_street"

$Query = @"
[out:json][timeout:90];
(
  way["highway"~"^($HighwayFilter)$"]
     (around:$RadiusM,$CenterLat,$CenterLon);
);
out body;
>;
out skel qt;
"@

function Haversine($lat1, $lon1, $lat2, $lon2) {
    $R  = 6371000.0
    $p1 = [Math]::PI * [double]$lat1 / 180
    $p2 = [Math]::PI * [double]$lat2 / 180
    $dp = [Math]::PI * ([double]$lat2 - [double]$lat1) / 180
    $dl = [Math]::PI * ([double]$lon2 - [double]$lon1) / 180
    $a  = [Math]::Sin($dp/2)*[Math]::Sin($dp/2) + [Math]::Cos($p1)*[Math]::Cos($p2)*[Math]::Sin($dl/2)*[Math]::Sin($dl/2)
    return 2 * $R * [Math]::Asin([Math]::Sqrt($a))
}

Write-Host "Querying Overpass API..."
$Client = [System.Net.WebClient]::new()
$Client.Headers.Add("Content-Type", "application/x-www-form-urlencoded")
$Client.Headers.Add("User-Agent", "DroneChallenger/1.0")
$Encoded  = "data=" + [System.Uri]::EscapeDataString($Query)
$RawBytes = $Client.UploadData("https://overpass-api.de/api/interpreter", "POST",
    [System.Text.Encoding]::UTF8.GetBytes($Encoded))
$JsonText = [System.Text.Encoding]::UTF8.GetString($RawBytes)
$Response = $JsonText | ConvertFrom-Json

# Use string keys to avoid int32/int64 type-mismatch in PowerShell hashtables
$RawNodes = @{}
foreach ($el in $Response.elements) {
    if ($el.type -eq "node") {
        $key = [string]$el.id
        $RawNodes[$key] = @{ lat = [double]$el.lat; lon = [double]$el.lon }
    }
}
Write-Host "OSM nodes fetched: $($RawNodes.Count)"

$RawEdges    = [System.Collections.Generic.List[object]]::new()
$UsedNodeIds = [System.Collections.Generic.HashSet[string]]::new()

foreach ($el in $Response.elements) {
    if ($el.type -ne "way" -or $null -eq $el.nodes) { continue }
    $wayNodes = $el.nodes
    for ($i = 0; $i -lt $wayNodes.Count - 1; $i++) {
        $aKey = [string]$wayNodes[$i]
        $bKey = [string]$wayNodes[$i + 1]
        if (-not $RawNodes.ContainsKey($aKey) -or -not $RawNodes.ContainsKey($bKey)) { continue }
        $na = $RawNodes[$aKey]; $nb = $RawNodes[$bKey]
        $dA = Haversine $CenterLat $CenterLon $na.lat $na.lon
        $dB = Haversine $CenterLat $CenterLon $nb.lat $nb.lon
        if ($dA -gt $RadiusM -and $dB -gt $RadiusM) { continue }
        $len = Haversine $na.lat $na.lon $nb.lat $nb.lon
        $RawEdges.Add(@{ from=$aKey; to=$bKey; len=$len })
        $RawEdges.Add(@{ from=$bKey; to=$aKey; len=$len })
        [void]$UsedNodeIds.Add($aKey)
        [void]$UsedNodeIds.Add($bKey)
    }
}

$Sorted = $UsedNodeIds | Sort-Object { [long]$_ }
$IdMap  = @{}
$seq    = 1
foreach ($id in $Sorted) { $IdMap[$id] = $seq; $seq++ }

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$NodesPath = "$OutDir\nodes.csv"
$EdgesPath = "$OutDir\edges.csv"

$nodeLines = [System.Collections.Generic.List[string]]::new()
$nodeLines.Add("node_id,lon,lat")
foreach ($id in $Sorted) {
    $n = $RawNodes[$id]
    $nodeLines.Add("$($IdMap[$id]),$("{0:F7}" -f $n.lon),$("{0:F7}" -f $n.lat)")
}
[System.IO.File]::WriteAllLines($NodesPath, $nodeLines)

$edgeLines = [System.Collections.Generic.List[string]]::new()
$edgeLines.Add("from,to,length")
foreach ($e in $RawEdges) {
    $edgeLines.Add("$($IdMap[$e.from]),$($IdMap[$e.to]),$("{0:F3}" -f $e.len)")
}
[System.IO.File]::WriteAllLines($EdgesPath, $edgeLines)

Write-Host "nodes.csv : $($UsedNodeIds.Count) nodes  -> $NodesPath"
Write-Host "edges.csv : $($RawEdges.Count) directed edges -> $EdgesPath"
Write-Host "Done. Restart PIE to pick up the new graph."
