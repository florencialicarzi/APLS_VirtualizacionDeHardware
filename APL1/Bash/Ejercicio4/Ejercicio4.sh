#!/bin/bash

function ayuda() {
    echo "Esta es la ayuda del script."
    echo "-d | --directorio : Directorio de entrada para monitorear."
    echo "-s | --salida     : Directorio donde se guardarán los backups."
    echo "-h | --help        : Muestra esta ayuda."
    echo "-k | --kill        : Mata un proceso de monitoreo activo."
}

declare -A arrayArchivos

options=$(getopt -o d:s:hk --long directorio:,salida:,help,kill -- "$@" 2>/dev/null)

if [[ $? -ne 0 ]]; then
    printf "Los parámetros ingresados son incorrectos. Use -h | --help para ayuda.\n" >&2
    exit 1
fi

eval set -- "$options"

if [ "$1" == "--" ]
then
    echo "Error en los parametros, utilice -h | --help para ver la ayuda del script."
    exit 1
fi   


kill_variable=false

while true; do
    case "$1" in
        -d|--directorio)
            dirEntrada=$(realpath "$2")
            if [[ ! -d "$dirEntrada" ]]; then
                printf "El directorio de entrada '%s' no existe.\n" "$dirEntrada" >&2
                exit 1
            fi
            shift 2
            ;;
        -s|--salida)
            dirSalida=$(realpath "$2")
            if [[ ! -d "$dirSalida" ]]; then
                printf "El directorio de salida '%s' no existe.\n" "$dirSalida" >&2
                exit 1
            fi
            shift 2
            ;;
        -h|--help)
            ayuda
            exit 0
            ;;
        -k|--kill)
            kill_variable=true
            shift
            ;;
        --)
            shift
            break
            ;;
        *)
            printf "Error, revise la ayuda con -h | --help\n" >&2
            exit 1
            ;;
    esac
done

pid_inotify=$(pgrep -af "inotifywait.*$dirEntrada" | grep -w "$dirEntrada" | awk '{print $1}' | head -n 1)

if [ "$kill_variable" = true ]
then

    if [ -z "$dirEntrada" ]
    then
        echo "Debe especificar el directorio para terminar el monitoreo."
        exit 1
    fi

    if [[ -n "$pid_inotify" ]]
    then
        printf "Terminando el proceso de monitoreo con PID: %s en el directorio: %s\n" "$pid_inotify" "$dirEntrada"
        kill "$pid_inotify"
        exit 0
    else
        printf "No hay procesos de monitoreo activos para '%s'.\n" "$dirEntrada" >&2
        exit 1
    fi
fi

if [[ -n "$pid_inotify" ]]
then
    printf "Ya existe un monitoreo activo en '%s'.\n" "$dirEntrada" >&2
    exit 1
fi

if [[ -z "$dirEntrada" || -z "$dirSalida" ]]
then
    printf "Debe especificar los directorios de entrada (-d) y salida (-s).\n" >&2
    exit 1
fi

printf "Cargando archivos previos...\n"

while IFS= read -r archivo
do
    nombre_arch=$(basename "$archivo")
    tam=$(stat --format="%s" "$archivo")

    clave="${tam}_${nombre_arch}"

    arrayArchivos["$clave"]="$archivo"

done < <(find "$dirEntrada" -type f)

printf "Carga inicial completada.\n"

inotifywait -m -r -e create --format '%w%f' "$dirEntrada" | while read -r archivo
do
    directorio=$(dirname "$archivo")
    nombre=$(basename "$archivo")
    tam=$(stat --format="%s" "$archivo" 2>/dev/null || echo 0)

    clave="${tam}_${nombre}"

    if [[ -n "${arrayArchivos["$clave"]}" ]] 
    then
        nombreBackup=$(date "+%Y%m%d-%H%M%S")
        dirBackup="${dirSalida}/${nombreBackup}"

        printf "Archivo duplicado: %s\n" "$archivo"
        mkdir -p "$dirBackup"
        printf "Archivo duplicado de: %s\n" "${arrayArchivos["$clave"]}" > "$dirBackup/archivo_log.txt"

        mv "$archivo" "$dirBackup"
        tar -czf "${dirBackup}.tar.gz" -C "$dirSalida" "$nombreBackup"
        rm -rf "$dirBackup"

    else
        arrayArchivos["$clave"]="$archivo"
        printf "Nuevo archivo procesado: %s\n" "$archivo"
    fi

done &


