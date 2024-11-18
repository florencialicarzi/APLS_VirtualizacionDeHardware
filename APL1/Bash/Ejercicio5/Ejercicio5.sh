#!/bin/bash

function cargarArrayPersonajes() {

    if [ -f archivoPersonajes.JSON ] # Si el archivo de personajes existe, tengo algo ya cacheado, entonces cargo el array.
    then
        while IFS= read -r linea
        do
            id=$(echo "$linea" | jq -r '.Id')
            arrayPersonajes["$id"]="$linea" 
        done < <(jq -c '.[]' archivoPersonajes.JSON) 
    fi
}

function cargarArrayFilms() {

    if [ -f archivoFilms.JSON ]
    then
    while IFS= read -r linea
    do
        id=$(echo "$linea" | jq -r '.Id')
        arrayFilms["$id"]="$linea"
    done < <(jq -c '.[]' archivoFilms.JSON)    

    fi
}

function formatearJsonPersonajes() {

    if [ ! -f archivoPersonajes.JSON ] 
    then
        echo "[" > archivoPersonajes.JSON  # Inicia el archivo si no existe
        json_personajes=$(<archivoPersonajes.JSON)
    else
        # Si el archivo ya existe, elimina la última línea (el cierre del arreglo)
        json_personajes=$(<archivoPersonajes.JSON)
        sed -i '$d' archivoPersonajes.JSON
        sed -i '$ s/$/,/' archivoPersonajes.JSON
    fi
}

function formatearJsonFilms() {

    if [ ! -f archivoFilms.JSON ]
    then
        echo "[" > archivoFilms.JSON
        json_films=$(<archivoFilms.JSON)
    else
        json_films=$(<archivoFilms.JSON)
        sed -i '$d' archivoFilms.JSON
        sed -i '$ s/$/,/' archivoFilms.JSON
    fi
}

function finProcesamientoPersonajes() {
    # Elimina la última coma y cierra el arreglo
    sed -i '$ s/,$//' archivoPersonajes.JSON  # Elimina la última coma
    echo "]" >> archivoPersonajes.JSON   # Cierra el arreglo
}

function finProcesamientoFilms() {

    sed -i '$ s/,$//' archivoFilms.JSON
    echo "]" >> archivoFilms.JSON
}

function ayuda() {

    echo "Esta es la ayuda del script:"
    echo "Ten en cuenta que para que el script pueda ejecutar se necesita tener -> jq <- instalado"
    echo "Los parametros permitidos son -p | --people \ -f | --film \ -h | --help"
    echo "Los valores permitidos son unicamente numericos, en cualquier otro caso, no existira registro."
}

# INICIO DEL SCRIPT.

options=$(getopt -o p:f:h --l people:,film:,help: -- "$@" 2> /dev/null)

if [ "$?" != "0" ]
then
    echo "La opcion ingresada es incorrecta."
    exit 1
fi

eval set -- "$options"

if [ "$1" == "--" ]
then
    echo "Error en los parametros, revise la ayuda con -h | --help."
    exit 1
fi

# Como me puede llegar mas de 1 id, vamos a procesar la cadena completa.

while true
do
    case "$1" in
        -p | --people)

            echo " "
            echo "Personajes:"
            echo " " 
            
            declare -A arrayPersonajes

            cargarArrayPersonajes

            if  echo "$2" | grep -q ","; # Si encuentro una "," quiere decir que me llegaron multiples id's.
                then

                    IFS=',' read -r -a ids <<< "$2" # Meto en el array "ids" todos los valores que me llegaron por parametro.

                    for id in "${ids[@]}" # Para cada id en mi array, lo busco o en mi array de Personajes, y si no esta, llamo a la API.
                    do
                        if [ -n "${arrayPersonajes["$id"]}" ] # Si ese id existe, lo muestro.
                        then
                            echo "${arrayPersonajes["$id"]}" | jq .
    
                        else # Si no existe, llamo a la API.
                            response=$(wget -qO- swapi.dev/api/people/"$id")
                            
                            if [ "$?" -eq "0" ] # Si lo que me devuelve es 0, la solicitud fue exitosa.
                            then
                                personaje=$(echo "$response" | jq -c --arg id "$id" '{Id:$id, Name:.name, Gender:.gender, Height:.height, Mass:.mass, Birth_Year:.birth_year}')
                                echo "$personaje" | jq .
                                arrayPersonajes["$id"]="$personaje"
                                # Si entre por aca se que el personaje no estaba cacheado, entonces lo agrego al array para despues cachearlo.

                            else
                                echo "El id "$id" es incorrecto, no existe ningun registro con dicho valor."
                                echo " "
                            fi
                        fi
                    done

            else # Si no encontre una "," quiere decir que me llego un unico id, y procedo de la misma forma.
                id="$2"

                if [ -n "${arrayPersonajes["$id"]}" ] # Busco en el array que sirve de cache.
                then
                    echo "${arrayPersonajes["$id"]}" | jq .
                else # Caso contrario llamo a la API.

                    response=$(wget -qO- swapi.dev/api/people/"$id") 

                    if [ "$?" -eq "0" ] # Si la solicitud fue exitosa, lo muestro y me guardo el personaje para luego utilizarlo de cache.
                    then
                        personaje=$(echo "$response" | jq -c --arg id "$id" '{Id:$id, Name:.name, Gender:.gender, Height:.height, Mass:.mass, Birth_Year:.birth_year}')
                        echo "$personaje" | jq .
                        arrayPersonajes["$id"]="$personaje"
                    else
                        echo "El id "$id" es incorrecto, no existe ningun registro con dicho valor."
                        echo " "

                    fi
                fi
            fi

        formatearJsonPersonajes
        
        for personaje in "${arrayPersonajes[@]}"
        do
            id=$(echo "$personaje" | jq -r '.Id')
            if ! echo "$json_personajes" | jq -e ".[] | select(.Id == \"$id\")" &> /dev/null
            then
                echo "$personaje," >> archivoPersonajes.JSON
            fi
        done

        finProcesamientoPersonajes


        shift 2 # Shifteo los parametros en caso de que tenga peliculas. $1 pasara a tener -f|--film y $2 los ids.

        ;;
        -f | --film)

            echo " "
            echo "Peliculas:"
            echo " "

            declare -A arrayFilms

            cargarArrayFilms

            if  echo "$2" | grep -q ",";
                then

                    IFS=',' read -r -a ids <<< "$2"

                    for id in "${ids[@]}"
                    do

                        if [ -n "${arrayFilms["$id"]}" ]
                        then
                            echo "${arrayFilms["$id"]}" | jq .
                        else

                            response=$(wget -qO- swapi.dev/api/films/"$id")

                            if [ "$?" -eq "0" ]
                            then
                                film=$(echo "$response" | jq --arg id "$id" '{Id:$id, Title:.title, Episode_id:.episode_id, Release_date:.release_date, Opening_crawl:.opening_crawl}')
                                echo "$film" | jq .
                                arrayFilms["$id"]="$film"
                            else
                                echo "El id "$id" es incorrecto, no existe ningun registro con ese valor."
                                echo " "
                            fi 
                        fi

                    done

            else
                id="$2"

                if [ -n "${arrayFilms["$id"]}" ]
                then
                    echo "${arrayFilms["$id"]}" | jq .
                else
                    response=$(wget -qO- swapi.dev/api/films/"$id")

                    if [ "$?" -eq "0" ]
                    then
                        film=$(echo "$response" | jq --arg id "$id" '{Id:$id, Title:.title, Episode_id:.episode_id, Release_date:.release_date, Opening_crawl:.opening_crawl}')
                        echo "$film" | jq .
                        arrayFilms["$id"]="$film"
                    else
                        echo "El id "$id" es incorrecto, no existe un registro con dicho valor."
                        echo " "
                    fi
                fi
            fi

        formatearJsonFilms

        for film in "${arrayFilms[@]}"
        do  
            id=$(echo "$film" | jq -r '.Id')
            if ! echo "$json_films" | jq -e ".[] | select(.Id == \"$id\")" &> /dev/null
            then
                echo "$film," >> archivoFilms.JSON
            fi
        done

        finProcesamientoFilms

        shift 2
        ;;

        --)
            shift
            break
        ;;

        -h | --help)
            ayuda
            exit 0
        ;;
    esac
done


echo "El procesamiento fue exitoso."










