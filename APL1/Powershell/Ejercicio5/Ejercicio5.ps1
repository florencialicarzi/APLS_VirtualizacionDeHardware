<#
.SYNOPSIS
    Este script consulta información sobre personajes y películas de Star Wars utilizando la API de swapi.tech.

.DESCRIPTION
    El script permite buscar información de personajes y películas por sus ID, con la opción de enviar múltiples IDs para ambos parámetros.
    La información obtenida se almacena en caché y se muestra en un formato legible. En caso de IDs inválidos, se informa al usuario con un mensaje adecuado.

.PARAMETER -People
    Especifica los ID de los personajes a buscar. Se permite ingresar múltiples IDs como un array. 

.PARAMETER -Film
    Especifica los ID de las películas a buscar. Se permite ingresar múltiples IDs como un array. 

.EXAMPLE
    ./swapi.ps1 -People 1,2 -Film 1,2
    Este comando buscará información para los personajes con ID 1 y 2, así como para las películas con ID 1 y 2.

.EXAMPLE
    ./swapi.ps1 -People 3
    Este comando buscará información para el personaje con ID 3 y mostrará los resultados en pantalla.

.NOTES
    Asegúrate de tener conexión a Internet para realizar consultas a la API.
#>



# 43895910 Gonzalez, Luca Sebastian
# 42597132 Gonzalez, Victor Matias
# 43458509 Licarzi, Florencia Berenice
# 42364617 Polito, Thiago

param (
    [Parameter (Mandatory = $true, ParameterSetName = 'people')]
    [Parameter (Mandatory = $true, ParameterSetName = 'both')]
    [int[]]$people, 

    [Parameter (Mandatory = $true, ParameterSetName = 'film')]
    [Parameter (Mandatory = $true, ParameterSetName = 'both')]
    [int[]]$film  
)

# Archivo de caché
$cachePeoplePath = "$PSScriptRoot\people_cache.json"
$cacheFilmPath = "$PSScriptRoot\film_cache.json"

# Cargar el caché si existe
$cachePeople = @{}
if (Test-Path $cachePeoplePath) {
    $jsonContent = Get-Content $cachePeoplePath -Raw
    $loadedPeopleCache = ConvertFrom-Json $jsonContent

    foreach ($key in $loadedPeopleCache.PSObject.Properties.Name) {
        $cachePeople[$key] = $loadedPeopleCache.$key
    }
}

$cacheFilm = @{}
if (Test-Path $cacheFilmPath) {
    $jsonContent = Get-Content $cacheFilmPath -Raw
    $loadedFilmCache = ConvertFrom-Json $jsonContent

    foreach ($key in $loadedFilmCache.PSObject.Properties.Name) {
        $cacheFilm[$key] = $loadedFilmCache.$key
    }
}

# Función para consultar la API de SWAPI para personajes
function Get-SwapiPerson {
    param (
        [string]$id
    )

    # Verificar en caché
    if ($cachePeople.ContainsKey($id)) {
        Write-Host "Recuperando de la caché: Personaje con ID $id"
        return $cachePeople[$id]
    }

    # URL de la API para personajes
    $url = "https://www.swapi.tech/api/people/$id"

    # Llamada a la API
    try {
        $response = Invoke-RestMethod -Uri $url -Method Get
        $person = $response.result

        # Guardar en caché solo los datos necesarios
        $shortPerson = @{
            uid = $person.uid
            name = $person.properties.name
            gender = $person.properties.gender
            height = $person.properties.height
            mass = $person.properties.mass
            birth_year = $person.properties.birth_year
        }

        # Actualizar caché y guardar
        $cachePeople[$id] = $shortPerson
        $cachePeople | ConvertTo-Json -Depth 10 | Set-Content $cachePeoplePath

        Write-Host "Contenido de la caché después de guardar: $($cachePeople | ConvertTo-Json -Depth 10)"
        return $shortPerson
    } catch {
        Write-Error "Error al consultar la API para el personaje con ID $id. Detalles: $_"
        return $null
    }
}

# Función para consultar la API de SWAPI para películas
function Get-SwapiFilm {
    param (
        [string]$id
    )

    # Verificar en caché
    if ($cacheFilm.ContainsKey($id)) {
        Write-Host "Recuperando de la caché: Película con ID $id"
        return $cacheFilm[$id]
    }

    # URL de la API para películas
    $url = "https://www.swapi.tech/api/films/$id"

    # Llamada a la API
    try {
        $response = Invoke-RestMethod -Uri $url -Method Get
        $film = $response.result
        
        # Guardar en caché solo los datos necesarios
        $shortFilm = @{
            title = $film.properties.title
            episode_id = $film.properties.episode_id
            release_date = $film.properties.release_date
            opening_crawl = $film.properties.opening_crawl
        }

        # Actualizar caché y guardar
        $cacheFilm[$id] = $shortFilm
        $cacheFilm | ConvertTo-Json -Depth 10 | Set-Content $cacheFilmPath

        return $shortFilm
    } catch {
        Write-Error "Error al consultar la API para la película con ID $id. Detalles: $_"
        return $null
    }
}

# Consultar personajes si se pasan como parámetro
if ($people) {
    foreach ($id in $people) {
        $pj = Get-SwapiPerson -id $id
        if ($pj) {
            Write-Output "Personaje:"
            Write-Output "Id: $($pj.uid)"
            Write-Output "Name: $($pj.name)"
            Write-Output "Gender: $($pj.gender)"
            Write-Output "Height: $($pj.height)"
            Write-Output "Mass: $($pj.mass)"
            Write-Output "Birth Year: $($pj.birth_year)"
            Write-Output "`n"
        } else {
            Write-Output "No se encontró información para el personaje con ID $id."
        }
    }
}

# Consultar películas si se pasan como parámetro
if ($film) {
    foreach ($id in $film) {
        $movie = Get-SwapiFilm -id $id
        if ($movie) {
            Write-Output "Película:"
            Write-Output "Title: $($movie.title)"
            Write-Output "Episode id: $($movie.episode_id)"
            Write-Output "Release date: $($movie.release_date)"
            Write-Output "Opening crawl: $($movie.opening_crawl)"
            Write-Output "`n"
        } else {
            Write-Output "No se encontró información para la película con ID $id."
        }
    }
}
