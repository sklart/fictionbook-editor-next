# Windows PowerShell 5.1 lacks ConvertFrom-Json -AsHashtable.
function ConvertTo-FbeHashtable {
    param([Parameter(ValueFromPipeline = $true)]$Value)
    process {
        if ($null -eq $Value) { return $null }
        if ($Value -is [string] -or $Value.PSObject.BaseObject -is [string]) { return [string]$Value }
        if ($Value -is [System.ValueType]) { return $Value }
        if ($Value -is [System.Collections.IEnumerable] -and -not ($Value -is [System.Collections.IDictionary])) {
            return @($Value | ForEach-Object { ConvertTo-FbeHashtable $_ })
        }
        if ($Value -is [System.Collections.IDictionary]) {
            $result = @{}
            foreach ($key in $Value.Keys) { $result[$key] = ConvertTo-FbeHashtable $Value[$key] }
            return $result
        }
        $result = @{}
        foreach ($property in $Value.PSObject.Properties) {
            $result[$property.Name] = ConvertTo-FbeHashtable $property.Value
        }
        return $result
    }
}
