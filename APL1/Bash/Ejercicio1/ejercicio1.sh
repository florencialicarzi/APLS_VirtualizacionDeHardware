#!/bin/bash

function ayuda() {
    cat <<EOF
Este script automatiza la validación de jugadas de lotería para diversas agencias. 
Procesa los archivos CSV que contienen las jugadas de la semana y valida si existen ganadores o quiénes obtuvieron premios secundarios (4 o 3 aciertos).

ARGUMENTOS:
  -d, --directory <dir>
    Directorio donde se encuentran los archivos CSV de las jugadas por agencia. El nombre de cada archivo indica la agencia.

  -a, --archivo
    Determina el archivo en el que se guardaran los resultados. Debe contener la ruta completa incluyendo el nombre del archivo.
    Esta opcion no puede ser usada en conjunto con '-p' o '--pantalla'

  -p, --pantalla
    Determina que los resultados del script seran mostrados por pantalla y no se guardaran. Esta opcion no puede ser usada en conjunto con '-a' o '--archivo'

Ejemplo:
  ./validate_lottery.sh -d /path/to/lottery_files -a /mnt/c/Users/User/Desktop/resultados.json

Nota:
  - Cada archivo CSV de agencia debe tener el formato: id, num1, num2, num3, num4, num5. No deben tener encabezado y deben terminar con un salto de linea.
  - Los números de las jugadas y los números ganadores deben estar en el rango de 0 a 99. Dicho archivo tampoco debe tener encabezado y terminar con un salto de linea.
  - El archivo JSON de salida se generará en la misma carpeta en la que se encuentra este script. Contendrá los resultados en la siguiente estructura:
    {
      "5_aciertos": [ ... ],
      "4_aciertos": [ ... ],
      "3_aciertos": [ ... ]
    }
    Donde cada lista contiene objetos con las claves "agencia" y "jugada".
EOF
}

salida=0
archivoSalida=''
options=$(getopt -o d:a:ph --l directorio:,archivo:,pantalla,help -- "$@" 2> /dev/null)
if [ "$?" != "0" ]
then
    echo 'Opciones incorrectas'
    exit 1
fi

eval set -- "$options"

if [ "$1" == "--" ]
then
    echo "Error en los parametros, utilice la ayuda con -h | --help."
    exit 1
fi

while true
do
    case "$1" in
        -d | --directorio)

            directorio=$(realpath "$2")

            if [ ! -d "$directorio" ]
            then
                echo "El directorio ingresado para el CSV no existe."
                exit 1
            fi

            shift 2
            ;;
        -a | --archivo)
            if [[ "$salida" -ne 0 ]]; then
                echo "Error: No se pueden seleccionar ambas opciones -a/--archivo y -p/--pantalla a la vez."
                exit 1
            fi
            archivoSalida="$2"
            salida=1
            shift 2
            ;;
        -p | --pantalla)

            if [[ "$salida" -ne 0 ]]; then
                echo "Error: No se pueden seleccionar ambas opciones -a/--archivo y -p/--pantalla a la vez."
                exit 1
            fi
            
            salida=2
            shift 1
            ;;
        -h | --help)
            ayuda 
            exit 0
            ;;
        --)
            shift
            break
            ;;
        *)
            echo "error"
            exit 1
            ;;
    esac
done

if [ -z "$directorio" ]
then
    echo "Debe ingresar un directorio para poder procesar la partida."
    exit 1
fi

if [ "$salida" == "0" ]
then
    echo "Debe ingresar alguna de las opciones de salida, ya sea -a para archivo o -p para mostrarlo por pantalla."
    exit 1
fi

ganadores="ganadores.csv"
if [[ ! -f $ganadores ]]; then
  echo "El archivo $ganadores no existe."
  exit 1
fi
read -r num_ganadores < "$ganadores"
echo "Números ganadores: $num_ganadores"
echo ""

cadenaFinal=""
aciertos5=""
aciertos4=""
aciertos3=""

for jugadas in "$directorio"/*.csv; do
    echo "Procesando archivo: $jugadas"

    while IFS=, read -r jugada num1 num2 num3 num4 num5
    do
      aciertos=0
        for numero in "$num1" "$num2" "$num3" "$num4" "$num5"
            do
                numero=$(echo "$numero" | tr -d '[:space:]')
                if [[ $num_ganadores =~ (^|,)$numero(,|$) ]]; then            
                    ((aciertos++))
                fi
       done
    agencia="${jugadas##*/}"
    agencia="${agencia%.*}"
    case "$aciertos" in
        5)
        if [[ "$aciertos5" == '' ]]; then
            aciertos5+="{\"agencia\": \"$agencia\",\"jugada\": \"$jugada\"}"
        else
            aciertos5+=",{\"agencia\": \"$agencia\",\"jugada\": \"$jugada\"}"
        fi
            ;;
        4)
            aciertos4+="{\"agencia\": \"$agencia\",\"jugada\": \"$jugada\"}"
            ;;
        3)
            aciertos3+="{\"agencia\": \"$agencia\",\"jugada\": \"$jugada\"}"
            ;;
        *)
            ;;
esac
    done < "$jugadas"
done
echo ""
json=$(jq -n \
  --argjson a5 "[$aciertos5]" \
  --argjson a4 "[$aciertos4]" \
  --argjson a3 "[$aciertos3]" \
  '{
    "5_aciertos": $a5,
    "4_aciertos": $a4,
    "3_aciertos": $a3
  }')

if [[ "$salida" -eq 1 ]]; then
    echo "$json" > "$archivoSalida"
    echo "Resultados generados en el archivo $archivoSalida"
else
    echo "Resultados finales:"
    formatted=$(printf "%s" "$json" | while IFS= read -r -n1 char; do
    case "$char" in
        '{' | '[')
            printf "%s\n" "$char"
            ;;
        '}' | ']')
            printf "\n%s" "$char"
            ;;
        ',')
            # Añadir una nueva línea si no está precedido por '}' o ']'
            if [[ ${output: -1} != "{" && ${output: -1} != "[" && ${output: -1} != "}" && ${output: -1} != "]" ]]; then
                printf ",\n"
            else
                printf ","
            fi
            ;;
        *)
            printf "%s" "$char"
            ;;
    esac
    done)
echo "$formatted"
fi

echo "Fin del script"