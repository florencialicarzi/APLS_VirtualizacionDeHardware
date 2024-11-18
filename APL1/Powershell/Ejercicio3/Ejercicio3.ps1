
# 43895910 Gonzalez, Luca Sebastian
# 42597132 Gonzalez, Victor Matias
# 43458509 Licarzi, Florencia Berenice
# 42364617 Polito, Thiago

<#
.SYNOPSIS
    Este script identifica archivos duplicados en un directorio, considerando su nombre y tamaño.

.DESCRIPTION
    El script escanea el directorio especificado y sus subdirectorios para encontrar archivos con el mismo nombre y tamaño. La salida se presenta como un listado de archivos duplicados, indicando las rutas donde fueron encontrados.

.PARAMETER -Directorio
    Ruta del directorio a analizar. Este parámetro es obligatorio.

.EXAMPLE
    ./identificarDuplicados.ps1 -Directorio "../analizar"
    Este comando analizará el directorio especificado y mostrará los archivos duplicados encontrados.

#>


param(
    [Parameter (Mandatory = $true)]
    [string]$directorio
)

# Validar el directorio
if (-not (Test-Path $directorio)) {
    Write-Error "El directorio especificado no existe."
    exit
}

#Obtengo el listado de archivos
$res_archivos = Get-ChildItem -Recurse -File -Path $directorio

#Creo diccionario: Clave compuesta-> [nombre-tam] Valor->[Paths]
$diccionario_arch = @{}


foreach ($arch in $res_archivos){
    $clave = "$($arch.Name)|$($arch.Length)"
    
    if($diccionario_arch.ContainsKey($clave)){
        $diccionario_arch[$clave].Add($arch.Directory) > $null
    }else{
        $diccionario_arch[$clave] = [System.Collections.ArrayList]::new()
        $diccionario_arch[$clave].Add($arch.Directory) > $null
    }
}




$clavesAEliminar = [System.Collections.ArrayList]::new();

Write-Host "Duplicados:"

foreach ($key in $diccionario_arch.Keys) {
    if ($diccionario_arch[$key].Count -gt 1) {

        $nombre = $key -split '\|' | Select-Object -First 1
        Write-Host "$nombre"

        foreach ($path in $diccionario_arch[$key]) {
            Write-Host "    $path"
        }
        Write-Host " "
    }
    else {
        $clavesAEliminar.Add($key) > $null
    }
}

foreach($key in $clavesAEliminar){
    $diccionario_arch.Remove($key)
}

$diccionario_arch
