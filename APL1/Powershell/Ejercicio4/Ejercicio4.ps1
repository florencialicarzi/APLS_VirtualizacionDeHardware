# 43895910 Gonzalez, Luca Sebastian
# 42597132 Gonzalez, Victor Matias
# 43458509 Licarzi, Florencia Berenice
# 42364617 Polito, Thiago

<#
.SYNOPSIS
    Este script monitorea un directorio en busca de archivos duplicados y genera copias de seguridad en formato ZIP.

.DESCRIPTION
    El script se ejecuta en segundo plano, validando continuamente si se están generando archivos duplicados en el directorio especificado (-directorio) y sus subdirectorios. En caso de detectar un archivo duplicado, se registra un log y se archiva el archivo en un archivo comprimido con formato ZIP en el path especificado (-salida).
    El script permite iniciar y detener el monitoreo según sea necesario.

.PARAMETER -Directorio
    Ruta del directorio a monitorear. Este parámetro es obligatorio y debe ser único por instancia del script.

.PARAMETER -Salida
    Ruta del directorio donde se crearán los archivos de backup comprimidos. Este parámetro solo se puede usar junto con -Directorio. 

.PARAMETER -Kill
    Switch que indica que el script debe detener la ejecucion previamente iniciada. Este parámetro solo se puede usar junto con -Directorio.

.EXAMPLE
    ./duplicados.ps1 -Directorio "../monitor" -Salida "../salida"
    Este comando iniciará el monitoreo del directorio especificado en segundo plano, generando backups en el directorio de salida en caso de detectar archivos duplicados.

.EXAMPLE
    ./duplicados.ps1 -Directorio "../monitor" -Kill
    Este comando detendrá el proceso demonio que está monitoreando el directorio especificado.

.NOTES
    - Solo puede haber una instancia del demonio ejecutándose para cada directorio.
    - El formato de los nombres de los archivos de backup es "yyyyMMdd-HHmmss.zip".
#>


Param (

    [ValidateNotNullOrEmpty()]
    [string]$directorio,

    [string]$salida,

    [switch]$kill = $false
#
)

# Validación: Asegurarse de que el directorio sea proporcionado
if (-not $directorio) {
    Write-Error "El parámetro '-directorio' es obligatorio."
    if ($salida -and $kill) {
        Write-Error "No puede usar '-salida' y '-kill' juntos. Use uno u otro según la acción deseada."
        exit 1
    }
    exit 1
}

# Validación: Si se proporciona solo '-directorio' sin '-salida' ni '-kill'
if ($directorio -and -not ($salida -or $kill)) {
    Write-Error "Debe proporcionar el parámetro '-salida' (para iniciar el monitoreo) o '-kill' (para detenerlo)."
    exit 1
}

# Validación: Si se proporcionan '-salida' y '-kill' al mismo tiempo
if ($salida -and $kill) {
    Write-Error "No puede usar '-salida' y '-kill' juntos. Use uno u otro según la acción deseada."
    exit 1
}

# Validación: Asegurarse de que el directorio proporcionado exista
if (-not (Test-Path $directorio)) {
    Write-Error "El Path a monitorear enviado por parámetro no existe."
    exit 1
}

# Lógica de ejecución según los parámetros
if ($kill) {
    Write-Output "Finalizando monitoreo..."
    
} elseif ($salida) {
    Write-Output "Iniciando monitoreo en el directorio: $directorio"
    Write-Output "Los Backups se guardarán en: $salida"
    
} else {
    Write-Error "Error inesperado: Parámetros no válidos."
    exit 1
}

$directorio = (Resolve-Path -Path $directorio).Path

# Función para verificar si un directorio ya está siendo monitoreado
function VerificarMonitoreo {
    param ($directorio)
    return Get-EventSubscriber | Where-Object {
        $_.SourceObject.Path -eq $directorio -and $_.EventName -eq 'Created'
    }
}

# Lógica para detener monitoreo con el flag -kill
if ($kill) {
    $suscriptor = VerificarMonitoreo -directorio $directorio
    if ($null -ne $suscriptor) {
        $suscriptor | Unregister-Event
        Write-Output "Monitoreo detenido para el directorio $directorio."
    } else {
        Write-Output "No se encontró un monitoreo activo para el directorio $directorio."
    }
    exit
}

# Validar si el directorio ya está siendo monitoreado
if (VerificarMonitoreo -directorio $directorio) {
    Write-Output "Ya existe un proceso monitoreando el directorio $directorio."
    exit 1
}


if( -not (Test-Path $salida)) {
    Write-Output "El Path donde se crean los logs enviado por parametro no existe."
    exit 1
}
New-Object System.IO.FileSystemWatcher | Get-Member -Type Event | Select-Object Name

# 1 - InputObject
# 2 - EventName -> Evento asociado al objeto.
# 3 - SourceIdentifier
# 4 - Action

$watcher = New-Object System.IO.FileSystemWatcher


$watcher.Path = $directorio

$watcher.NotifyFilter = [System.IO.NotifyFilters]'FileName'

$watcher.Filter = "*"

$watcher.IncludeSubdirectories = $true


$messageData = [PSCustomObject]@{
    PathLog    = $salida
    Directorio = $directorio
}
 
$action = {
    #*PARAMETROS DEL CONTEXTO
    $PathLog = $event.MessageData.PathLog
    $directorio = $event.MessageData.Directorio

    #*PARAMETROS DEL EVENTO
    $filePath = $Event.SourceEventArgs.FullPath   # Ruta completa del archivo
    $fileName = Split-Path -Path $Event.SourceEventArgs.FullPath -Leaf
    $fileSize = (Get-Item -Path $filePath).Length # Tamaño del archivo en bytes

    #*TIMESTAMP
    $timestamp = (Get-Date).ToString("yyyyMMdd-HHmmss")

    #*LOGICA DE DUPLICADOS
    $diccionario_arch = @{}

    #Obtengo el listado de archivos
    $res_archivos = Get-ChildItem -Recurse -File -Path $directorio

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

    foreach ($key in $diccionario_arch.Keys) {
        if ($diccionario_arch[$key].Count -lt 2) {
            $clavesAEliminar.Add($key) > $null
        }
    }

    foreach($key in $clavesAEliminar){
        $diccionario_arch.Remove($key)
    }

    $claveIsDup = "$fileName|$fileSize"


    if($diccionario_arch.ContainsKey($claveIsDup))
    {
        #*CREACION ZIP
    
        # Crear una carpeta para almacenar el archivo de log
        $logFolder = Join-Path -Path $PathLog -ChildPath "BackUpLog-$timestamp"
        if (-not (Test-Path $logFolder)) {
            New-Item -Path $logFolder -ItemType Directory | Out-Null
        }
    
        # Crear el archivo de log dentro de la carpeta
        $logFile = Join-Path -Path $logFolder -ChildPath "log-$timestamp.txt"
        "Duplicado: $fileName Peso: $fileSize Creacion:$filePath $((Get-Date).ToString("yyyy-MM-dd HH:mm:ss")) Monitoreado en: $directorio" | Out-File -FilePath $logFile -Encoding UTF8

        #Mover el archivo duplicado
        $newFilePath = Join-Path -Path $logFolder -ChildPath $fileName
        Move-Item -Path $filePath -Destination $newFilePath -Force
    
        # Crear el archivo ZIP de la carpeta
        $zipFile = Join-Path -Path $PathLog -ChildPath "$timestamp.zip"
        Compress-Archive -Path $logFolder -DestinationPath $zipFile
    
        # Eliminar la carpeta original después de comprimirla
        Remove-Item -Path $logFolder -Recurse -Force

    }



}

$leaf = Split-Path -Path $directorio -Leaf
$time = (Get-Date).ToString("yyyyMMdd-HHmmss")
$sourceIdentifier = "MonitorCreador|$leaf|$time"

Register-ObjectEvent -InputObject $watcher -EventName Created -SourceIdentifier $sourceIdentifier -Action $action -MessageData $messageData
    
$watcher.EnableRaisingEvents = $true

Start-Job -ScriptBlock {

    param($watcher, $sourceIdentifier)
        
    Wait-Event -SourceIdentifier $sourceIdentifier

} -ArgumentList $watcher, $sourceIdentifier


Get-EventSubscriber