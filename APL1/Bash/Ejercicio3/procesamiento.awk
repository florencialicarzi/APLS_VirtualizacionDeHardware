BEGIN {

    # Se ejecuta antes de que lleguen las lineas.

    print "Inicio de procesamiento de archivo en -> " dirIni
    print " "

}

/^(\/)/ && /:$/ {

    # Si entramos por aca, una linea cumplio con la expresion regular, por lo tanto la procesamos.
    # Si estamos aca es porque encontramos la direccion donde se encuentra el archivo.

    dir = $1
}

# Filtro los archivos

/^-/ {

    tam = $5
    nombreArch = $9
    clave = tam "_" nombreArch

    archivos[clave] = (archivos[clave] ? archivos[clave] ", " : "") dir

    # Cargamos un array asociativo, si la clave existe, agregamos al final la direccion encontrada.
}

END { 

    for(clave in archivos) {
        
        split(clave, claveSplit, "_");
        tam = claveSplit[1];
        nombreArch = claveSplit[2];

        if(index(archivos[clave], ",") != 0) {

            split(archivos[clave], directorios, ", ");

            print nombreArch

            for(i=1; i<=length(directorios); i++) {

                gsub(":", "", directorios[i]);
                print "\t -> " directorios[i]
            }
        }
    }
}