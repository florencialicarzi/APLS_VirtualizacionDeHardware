#!/bin/bash

producto=0
transponer=0
hayMatriz=0
haySeparador=0

# Función para mostrar ayuda
mostrar_ayuda() {
    echo "-------------------------------- MOSTRANDO AYUDA -----------------------------------"
    echo ""
    echo "-m | --matriz          Ruta del archivo de la matriz"
    echo "-p | --producto        Valor entero para multiplicar la matriz (producto escalar)"
    echo "-t | --transponer      Realizar la transposición de la matriz"
    echo "-s | --separador       Separador para utilizar en la matriz"
    echo ""
    echo "------------------------------------------------------------------------------------"
}

validar_matriz() {
    local file="$1"
    local separator="$2"

    # Verificar si el archivo existe
    if [[ ! -f "$file" ]]; then
        echo "Error: el archivo '$file' no existe."
        return 1
    fi

    # Leer el archivo línea por línea
    while IFS= read -r line || [[ -n "$line" ]]; do
        # Eliminar el último carácter de la línea
        line=$(echo "$line" | sed 's/.$//')

        # Detectar el separador usado en la línea
        local detected_separator=""
        if [[ "$line" =~ [^0-9]*([[:punct:]])[^0-9]* ]]; then
            detected_separator="${BASH_REMATCH[1]}"
        fi

        # Comparar con el separador proporcionado
        if [[ "$detected_separator" != "$separator" ]]; then
            echo "Error: el separador detectado ('$detected_separator') no coincide con el proporcionado ('$separator')."
            return 1
        fi

        # Leer cada elemento de la línea
        while IFS="$separator" read -r -a elements; do
            for element in "${elements[@]}"; do
                # Comprobar si el elemento es un número
                if [[ ! "$element" =~ ^-?[0-9]+$ ]]; then
                    echo "Error: '$element' no es un número válido."
                    return 1
                fi
            done
        done <<< "$line"
    done < "$file"

    return 0
}

cantidad_columnas_por_fila(){
    matriz="$1"
    separador="$2"
    columnas=-1

    while IFS= read -r fila || [[ -n "$fila" ]]; do
        IFS="$separador" read -r -a elementos <<< "$fila"
        num_columnas=${#elementos[@]}

        if [ "$columnas" -eq -1 ]; then
            columnas=$num_columnas
        elif [ "$num_columnas" -ne "$columnas" ]; then
            return 1
        fi
    done < "$matriz"

    return 0
}

es_numero() {
    local valor="$1"
    # Verificar si el valor es un número
    if [[ "$valor" =~ ^-?[0-9]+(\.[0-9]+)?$ ]]; then
        return 0  # Es un número
    else
        return 1  # No es un número
    fi
}



es_separador_valido() {
    local separador="$1"

    # Verificar que el separador no esté vacío
    if [[ -z $separador ]]; then
        echo "Error: El separador no puede estar vacío."
        return 1
    fi
    
    if [[ "$separador" =~ ^\"\".+\"\"$ ]]; then
        echo "Error: El separador no puede estar entre comillas dobles repetidas."
        return 1
    fi

    # Verificar que sea un solo carácter
    if [[ ${#separador} -ne 1 ]]; then
        echo "Error: El separador debe ser un solo carácter."
        return 1
    fi

    # Verificar que no sea un número ni el signo menos
    if [[ $separador =~ ^[0-9-]$ ]]; then
        echo "Error: El separador no puede ser un número ni el signo menos (-)."
        return 1
    fi

    return 0
}


transponer_matriz() {
    matriz="$1"
    separador="$2"
    archivo_salida="$3"
    declare -A matriz_asociativa
    filas=0
    columnas=0

    # Limpiar el archivo de salida si existe
    > "$archivo_salida"

    # Leer el archivo línea por línea y almacenar en una matriz asociativa
    while IFS= read -r fila || [[ -n "$fila" ]]; do
        # Limpiar cualquier salto de línea o espacio en blanco al final
        fila=$(echo "$fila" | sed 's/[[:space:]]*$//')

        # Dividir la fila en elementos usando el separador
        IFS="$separador" read -r -a elementos <<< "$fila"

        for i in "${!elementos[@]}"; do
            # Validar si el elemento es un número
            if es_numero "${elementos[i]}"; then
                matriz_asociativa["$filas,$i"]="${elementos[i]}"
            else
                echo "ERROR: Algunos elementos de la matriz de entrada no son válidos"
                exit 1
            fi
            
        done

        # Actualizar el número de columnas si es necesario
        if [ ${#elementos[@]} -gt "$columnas" ]; then 
            columnas=${#elementos[@]}
        fi

        ((filas++))
    done < "$matriz"

    # Mostrar la matriz transpuesta
    for ((i=0; i<columnas; i++)); do
        fila_transpuesta=""
        for ((j=0; j<filas; j++)); do
            valor="${matriz_asociativa[$j,$i]}"
            if [ -n "$valor" ]; then
                if [ -z "$fila_transpuesta" ]; then
                    fila_transpuesta="$valor"
                else
                    fila_transpuesta="$fila_transpuesta$separador$valor"
                fi
            fi
        done
        
        # Limpiar cualquier espacio en blanco residual
        fila_transpuesta=$(echo "$fila_transpuesta" | sed 's/[[:space:]]*$//')

        # Escribir la fila transpuesta solo si hay contenido
        if [ -n "$fila_transpuesta" ]; then
            echo "$fila_transpuesta" >> "$archivo_salida"
        fi
    done
}


producto_escalar() {
    escalar="$1"
    matriz="$2"
    separador="$3"
    archivo_salida="$4"
    resultado=""

    # Limpiar el archivo de salida si existe, para no sobrescribir datos anteriores
    > "$archivo_salida"
    
    # Leer el archivo línea por línea
    while read -r fila; do
        # Eliminar espacios en blanco alrededor de la línea
        fila=$(echo "$fila" | tr -d '[:space:]')

        # Separar los elementos de la fila usando el separador
        IFS="$separador" read -r -a elemento <<< "$fila"

        # Inicializar la nueva fila
        resultado_fila=""

        # Multiplicar cada elemento por el escalar y formar la nueva fila
        for elemento in "${elemento[@]}"; do
            # Validar si el elemento es un número (entero o decimal)
            if es_numero "$elemento"; then
                # Calcular el resultado de la multiplicación
                resultado=$(echo "$elemento * $escalar" | bc)
            else
                echo "ERROR: Algunos elementos de la matriz de entrada no son válidos"
                exit 1
            fi
            
            # Añadir el resultado a la fila, con el separador entre elementos
            if [ -z "$resultado_fila" ]; then
                resultado_fila="$resultado"
            else
                resultado_fila="$resultado_fila$separador$resultado"
            fi
        done

        # Escribir la fila procesada en el archivo de salida
        echo "$resultado_fila" >> "$archivo_salida"
    done < "$matriz"
}


options=$(getopt -o m:p:ts:h --l matriz:,producto:,transponer,separador:,help -- "$@" 2> /dev/null)
if [ $? -ne 0 ]; then
    echo "Opciones incorrectas"
    exit 1
fi

eval set -- "$options"
while true
do
    case "$1" in # switch ($1) { 
        -m | --matriz)
            hayMatriz=1
            matriz="$2"

            if [ ! -f "$matriz" ]
            then
                echo "El archivo no existe en el directorio enviado o no se especifico la ruta de un archivo."
                exit 1
            fi

            shift 2
            ;;
        -p | --producto)
            producto=1
            escalar="$2"
            shift 2 
            ;;
        -t | --transponer)
            transponer=1
            shift 1
            
            ;;
        -s | --separador)
            haySeparador=1
            separador="$2"
            shift 2  
            ;;
        -h | --help)
            mostrar_ayuda
            exit 1
            ;;
        --) # case "--":
             shift
             break
            ;;
        *) # default: 
            echo "error"
            exit 1
            ;;
    esac
done

# Validaciones
if [[ "$hayMatriz" -eq 0 ]]; then
    echo " Error: No se ha ingresado una matriz."
    exit 1
fi

if [[ "$transponer" -eq 1 && "$producto" -eq 1 ]]; then
    echo "Error: No se puede realizar la transposición y el producto escalar al mismo tiempo."
    exit 1
fi

if [[ ! -s "$matriz" ]]; then
    echo "El archivo de la matriz está vacío."
    exit 1
fi

validar_matriz "$matriz" "$separador"
if [[ $? -eq 1 ]]; then
    exit 1
fi

if [[ "$haySeparador" -eq 0 ]]; then
    echo "Error: No se ha indicado ningún separador."
    exit 1

fi

if [[ "$producto" -eq 1 ]]; then
    es_numero "$escalar"
    if [[ $? -eq 1 ]]; then 
         echo "Error: Usted ha ingresado un caracter que no es un número para calcular el producto escalar."
         exit 1
    fi
fi

if [[ "$haySeparador" -eq 1 ]]; then
    es_separador_valido "$separador"
    if [[ $? -eq 1 ]]; then 
         #echo "Error: Usted ha ingresado un separador inválido. Recuerde que tiene que ingresar un único caracter como separador, el cual no puede ser ni un número ni el signo '-'"
         exit 1
    fi
fi

cantidad_columnas_por_fila "$matriz" "$separador"
    if [[ $? -eq 1 ]]; then 
         echo "Error: La cantidad de columnas por fila no coinciden en la matriz de entrada"
         exit 1
    fi

archivo_salida="salida_$(basename "$matriz")"


if [[ "$transponer" -eq 1 ]]; then
    # Transponer la matriz
    transponer_matriz "$matriz" "$separador" "$archivo_salida"
    echo "La matriz transpuesta ha sido guardada en $archivo_salida"
    exit 1

elif [[ "$producto" -eq 1 ]]; then
        # Producto escalar
        producto_escalar "$escalar" "$matriz" "$separador" "$archivo_salida"
        echo "La matriz multiplicada por $escalar ha sido guardada en '$archivo_salida'."
        exit 1
else
        echo "Error: Usted no ha indicado si va a realizar una transposición o un producto escalar con su matriz."
        exit 1
    
    fi
fi








