<#
.SYNOPSIS
    Este script realiza el producto escalar y la trasposicion de matrices a partir de un archivo de texto plano.

.DESCRIPTION
    El script lee una matriz desde un archivo de texto y permite realizar operaciones de producto escalar o trasposicion. 
    La salida se guarda en un archivo llamado "salida.nombreArchivoEntrada" en el mismo directorio que el archivo de entrada. 
    Se realizan validaciones para matrices invalidas, incluyendo columnas desiguales y caracteres no numericos.

.PARAMETER -Matriz
    Ruta del archivo que contiene la matriz. Este parametro es obligatorio.

.PARAMETER -Producto
    Valor entero para utilizarse en el producto escalar. Este parametro no puede ser usado junto con -Trasponer.

.PARAMETER -Trasponer
    Indica que se debe realizar la operacion de trasposicion sobre la matriz. Este parametro no recibe valor adicional y no puede ser usado junto con -Producto.

.PARAMETER -Separador
    Caracter a utilizar como separador de columnas en el archivo de la matriz. Este parametro es opcional.

.EXAMPLE
    ./matrices.ps1 -Matriz "C:\ruta\matriz.txt" -Producto 2
    Este comando realizara el producto escalar de la matriz leida desde el archivo y lo multiplicara por 2, guardando el resultado en el archivo de salida correspondiente.

.EXAMPLE
    ./matrices.ps1 -Matriz "C:\ruta\matriz.txt" -Trasponer
    Este comando transpondra la matriz leida desde el archivo y guardara el resultado en el archivo de salida correspondiente.

.NOTES
    - Asegurate de que el archivo de entrada contenga una matriz valida.
    - El caracter separador puede ser cualquier caracter que no sea un numero o el simbolo menos (“-”).
#>


# Declarar parámetros
Param (
    [parameter(Mandatory=$false)]
    [string]$matriz,

    [double]$producto,

    [switch]$transponer,

    [parameter(Mandatory=$false)]
    [string]$separador,

    [switch]$help
)

# Función para mostrar ayuda
function Mostrar-Ayuda {
    Write-Output "-------------------------------- MOSTRANDO AYUDA -----------------------------------"
    Write-Output ""
    Write-Output "-matriz          Ruta del archivo de la matriz"
    Write-Output "-producto        Valor entero para multiplicar la matriz (producto escalar)"
    Write-Output "-transponer      Realizar la transposicion de la matriz"
    Write-Output "-separador       Separador para utilizar en la matriz"
    Write-Output "-help            Muestra esta ayuda"
    Write-Output ""
    Write-Output "EJEMPLOS DE USO:"
    Write-Output "    a) ./Ejercicio2.ps1 -matriz matriz.txt -separador '|' -producto -3"
    Write-Output "    b) ./Ejercicio2.ps1 -matriz matriz.txt -separador '|' -transponer"
    Write-Output "    c) ./Ejercicio2.ps1 -help"
    Write-Output "------------------------------------------------------------------------------------"
}

# Función para validar si el valor es numérico
function Es-Numero {
    param ([string]$valor)
    return ($valor -match '^-?\d+(\.\d+)?$')
}

# Función para validar la matriz
function Validar-Matriz {
    param (
        [string]$archivo,
        [string]$separador
    )

    # Verificar si el archivo existe
    if (-not (Test-Path $archivo)) {
        Write-Error "El archivo $archivo no existe."
        return $false
    }

    # Leer el contenido del archivo
    $contenido = Get-Content $archivo

    # Verificar si el archivo está vacío
    if (-not $contenido -or $contenido -cmatch '^\s*$') {
        Write-Error "Error: La matriz esta vacia o contiene solo espacios en blanco."
        return $false
    }

    # Verificar si el separador es correcto
    $primeraLinea = $contenido[0].Trim()
    if (-not $primeraLinea.Contains($separador)) {
        Write-Error "Error: El separador ingresado ('$separador') no coincide con el separador real de la matriz."
        return $false
    }

    # Validar el contenido de la matriz
    foreach ($linea in $contenido) {
        $linea = $linea.Trim()
        $elementos = $linea -split [regex]::Escape($separador)

        foreach ($elemento in $elementos) {
            if (-not (Es-Numero $elemento)) {
                Write-Error "Error: '$elemento' no es un numero valido."
                return $false
            }
        }
    }

    return $true
}




# Función para transponer la matriz
function Transponer-Matriz {
    param (
        [string]$archivo,
        [string]$separador,
        [string]$archivoSalida
    )

    $contenido = Get-Content $archivo
    $matriz = @()
    foreach ($linea in $contenido) {
        $matriz += ,($linea -split [regex]::Escape($separador))
    }

    $columnas = $matriz[0].Length
    $filas = $matriz.Length
    $resultado = @()

    for ($col = 0; $col -lt $columnas; $col++) {
        $fila = @()
        for ($filaIdx = 0; $filaIdx -lt $filas; $filaIdx++) {
            $fila += $matriz[$filaIdx][$col]
        }
        $resultado += ($fila -join $separador)
    }
    $resultado | Set-Content $archivoSalida
}

# Función para realizar el producto escalar
function Producto-Escalar {
    param(
        [double]$escalar,
        [string]$matriz,
        [string]$separador,
        [string]$archivoSalida
    )

    # Limpiar el archivo de salida si existe
    if (Test-Path $archivoSalida) {
        Clear-Content $archivoSalida
    }

    # Leer y procesar la matriz
    Get-Content $matriz | ForEach-Object {
        # Separar los elementos de la fila
        $elementos = $_ -split [regex]::Escape($separador)

        # Multiplicar cada elemento por el escalar
        $resultado_fila = $elementos | ForEach-Object { 
            if (Es-Numero $_) {
                [double]$_ * $escalar 
            } else {
                Write-Host "ERROR: El elemento '$_' no es un numero." -ForegroundColor Red
                exit 1
            }
        }

        # Escribir la fila procesada en el archivo de salida
        $resultado_fila -join $separador | Out-File -Append -FilePath $archivoSalida
    }
}


# Validaciones y ejecución
if ($help) {
    Mostrar-Ayuda
    exit 
}

if (-not $matriz) {
    Write-Error "Error: No se ha ingresado una matriz."
    exit 1
}

if ($transponer -and $producto) {
    Write-Error "Error: No se puede realizar la transposicion y el producto escalar al mismo tiempo."
    exit 1
}

if (-not (Test-Path $matriz)) {
    Write-Error "El archivo de la matriz no existe o esta vacio."
    exit 1
}

if (-not $separador) {
    Write-Error "Error: No se ha indicado ningun separador."
    exit 1
}

if (-not (Validar-Matriz -archivo $matriz -separador $separador)) {
    exit 1
}

# Ejecutar operaciones
$archivoSalida = "salida_" + (Get-Item $matriz).Name


if ($transponer) {
    Transponer-Matriz -archivo $matriz -separador $separador -archivoSalida $archivoSalida
    Write-Output "La matriz transpuesta ha sido guardada en $archivoSalida"
} elseif ($producto) {
    Producto-Escalar -escalar $producto -matriz $matriz -separador $separador -archivoSalida $archivoSalida
    Write-Output "La matriz multiplicada por $producto ha sido guardada en $archivoSalida"
} else {
    Write-Error "Error: Usted no ha indicado si va a realizar una transposicion o un producto escalar con su matriz."
    exit 1
}