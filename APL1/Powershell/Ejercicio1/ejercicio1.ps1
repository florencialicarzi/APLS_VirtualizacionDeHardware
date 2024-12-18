<#
.SYNOPSIS
    Este script procesa archivos CSV de jugadas y compara los números con una lista de números ganadores.
    
.DESCRIPTION
    Este script automatiza la validación de jugadas de lotería para diversas agencias. 
    Procesa los archivos CSV que contienen las jugadas de la semana y valida si existen ganadores o quiénes obtuvieron premios secundarios (4 o 3 aciertos).

.PARAMETER -Directorio
    Especifica el directorio donde se encuentran los archivos CSV de jugadas. El nombre de cada archivo indica la agencia. Este parámetro es obligatorio.

.PARAMETER -Archivo
    Especifica un archivo para procesar las jugadas. Debe contener la ruta completa incluyendo el nombre del archivo.
    Esta opcion no puede ser usada en conjunto con '-p' o '-pantalla'

.PARAMETER -Pantalla
    Especifica que los resultados deben mostrarse en pantalla. Este parámetro es opcional, pero si se usa, 
    no se puede usar el parámetro '-a' o '-Archivo' al mismo tiempo. Si no se selecciona ninguna opcion de entre -pantalla y -archivo, esta sera la opcion por defecto

.EXAMPLE
    .\ProcesarJugadas.ps1 -Directorio ".\archivos_agencias" -Archivo "./resultados.csv"
    Este comando procesará los archivos situados en el directorio especificado y guardara los resultados en un CSV con el nombre especificado.
    Recorda usar comillas al mandar un path

.EXAMPLE
    .\ProcesarJugadas.ps1 -directorio ".\Jugadas" -pantalla
    Este comando procesará el directorio 'jugadas' y mostrará los resultados en pantalla.
    Recorda usar comillas al mandar un path
#>

Param(
    [Parameter(Mandatory=$true, Position=0)]
    [string]$Directorio,

    [Parameter(Mandatory=$false)]
    [string]$Archivo = $null,

    [Parameter(Mandatory=$false)]
    [switch]$Pantalla
)

if (-not $Archivo -and -not $Pantalla) {
    Write-Error -Message "Debe especificar al menos el parámetro -Archivo o usar el interruptor -Pantalla junto con -Directorio."
    exit
}

if (($Archivo -ne '') -and ($Pantalla -eq 1)) {
    Write-Host "Error: No puede especificar -a (archivo) y -p (pantalla) juntos."
    exit 1
}

# Validación: Asegurarse de que el directorio proporcionado exista
if (-not (Test-Path $Directorio)) {
    Write-Error "El Path de Directorio enviado por parámetro no existe."
    exit 1
}

if ($Archivo) {
    # Asegurarse de que la ruta tenga solo barras normales
    $path = $Archivo -replace '\\', '/'   # Reemplaza \ por /

    # Obtener solo el directorio base
    if ($path.LastIndexOf('/') -gt 0) {
        $path = $path.Substring(0, $path.LastIndexOf('/'))
    } else {
        $path = "."  # Si no hay barra, significa que es solo un archivo sin directorio
    }

    Write-Output = "Directorio a crear->  $path"

    # Validar si el directorio existe
    if (-not (Test-Path -Path $path)) {
        Write-Error "El directorio para la creación del archivo no existe: $path"
        exit 1
    }
}



function Find-Numero() {
    param (
        [string]$archivo,
        [string]$numero
    )

    if (-not $archivo -or -not $numero) {
        Write-Host "Error, hubo un error al intentar encontrar el número $numero en el archivo $archivo"
        exit 1
    }
    Write-Host "archivo: $archivo"
    if (Get-Content $archivo | Select-String -Pattern $numero) {
        return $true  # Retorna verdadero si se encuentra el número
    } else {
        return $false  # Retorna falso si no se encuentra
    }
}



$ganadores = "ganadores.csv"
if (-not (Test-Path $ganadores)) {
    Write-Host "El archivo $ganadores no existe."
    exit 1
}

$num_ganadores = Get-Content $ganadores
Write-Host "Números ganadores: $num_ganadores`n"
$num_ganadores = $num_ganadores -split ',' | ForEach-Object { [int]$_ }

$aciertos5 = @()
$aciertos4 = @()
$aciertos3 = @()

foreach ($jugadas in Get-ChildItem "$directorio\*.csv") {
    Write-Host "Procesando archivo: $($jugadas.FullName)"

    Import-Csv $jugadas.FullName -Header "jugada", "num1", "num2", "num3", "num4", "num5" | ForEach-Object {
        $aciertos = 0
        foreach ($numero in $_.num1, $_.num2, $_.num3, $_.num4, $_.num5) {
            #if (Find-Numero $num_ganadores $numero) {
            if ($num_ganadores -contains $numero) {
                $aciertos++
            }
        }

        $agencia = [System.IO.Path]::GetFileNameWithoutExtension($jugadas.Name)
        $jugadaActual = $_.Jugada
        switch ($aciertos) {
            5 {
                $aciertos5 += @{ "agencia" = $agencia; "jugada" = $jugadaActual }
                break
            }
            4 {
                $aciertos4 += @{ "agencia" = $agencia; "jugada" = $jugadaActual }
                break
            }
            3 {
                $aciertos3 += @{ "agencia" = $agencia; "jugada" = $jugadaActual }
                break
            }
        }
    }
}

$jsonResult = [PSCustomObject]@{
    "5_aciertos" = $aciertos5
    "4_aciertos" = $aciertos4
    "3_aciertos" = $aciertos3
} | ConvertTo-Json -Depth 10

if ($Archivo -ne '') {
    $jsonResult | Out-File -FilePath $Archivo -Encoding utf8
    Write-Host "Resultados generados en el archivo $Archivo"
} 
else
{
    if($Pantalla) {
        Write-Host "Resultados finales:"
        $formatted = $jsonResult | Out-String
        Write-Host $formatted
    }
} 

Write-Host "Fin del script"