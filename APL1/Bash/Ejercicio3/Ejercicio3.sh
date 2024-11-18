#!/bin/bash

function ayuda() {

    echo "Esta es la ayuda del script:"
    echo "Compruebe que este colocando correctamente los parametros -d /directorio.. | --directorio /directorio.."
    echo "Tenga en cuenta que el archivo awk y el script deben de estar en el mismo directorio."
}

options=$(getopt -o d:h --l directorio:,help -- "$@" 2> /dev/null)

if [ "$?" != "0" ]
then
    echo "La opcion ingresada es incorrecta, pruebe con -d | -h | --directorio | --help"
    exit 1
fi

eval set -- "$options"

if [ "$1" == "--" ]
then
    echo "Utilice la ayuda para saber como enviar los parametros correctamente (-h)."
    exit 1
fi

if [ "$1" != "-h" ]
then
    dir="$(pwd)"
    awkScript="$dir/procesamiento.awk"
    directorio=$(realpath "$2")
fi

while true
do
    case "$1" in
        -d | --directorio)
            if [[ ! -d "$2" ]]
            then
                echo "El directorio ingresado no existe, utilice un directorio existente." >&2
                exit 1
            fi
            
            ls "$directorio" -lSR | awk  -v dirIni="$directorio" -f "$awkScript"

            shift 2
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
        echo "Error."
        exit 1
    ;;

    esac
done
