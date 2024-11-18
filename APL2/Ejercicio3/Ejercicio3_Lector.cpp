// 43895910 Gonzalez, Luca Sebastian
// 43458509 Licarzi, Florencia Berenice
// 42597132 Gonzalez, Victor Matias
// 42364617 Polito, Thiago

#include <iostream>
#include <fstream>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <filesystem>

using namespace std;

#define NombreMemoria "miMemoria"
#define mutexSemaforo "mtxSemaforo"

void procesarParametros(int argc, char* argv[], int& cantMensajes, string& numSensor, int& cantSegundos, string& archIds)
{
    
    if(string(argv[1]) == "-h" || string(argv[1]) == "--help")
    {
        cout << "Esta es la ayuda del script, asegurese de que este ingresando todos los parametros correctamente." << "\n" <<
                 "-i | --ids ./pepe.txt o /path1/path2/pepe.txt / -n | --numero 5 / -m | --mensajes 8 / -s | --segundos 3" << "\n" <<
                 "Todos estos parametros son requeridos para que el programa pueda funcionar correctamente." << endl;
        exit(0);
    }
    else if (argc < 9) { // Cambié a 9 para que haya suficientes parámetros para todos
        perror("La cantidad de parámetros es insuficiente.");
        exit(1);
    }

    for (int i = 1; i < argc; i++) {
        if (string(argv[i]) == "-s" || string(argv[i]) == "--segundos") {
            if (i + 1 < argc) { // Verifica que haya un siguiente argumento
                cantSegundos = stoi(argv[++i]);
            } else {
                cerr << "Error: Se esperaba un valor para -s/--segundos." << endl;
                exit(1);
            }
        } else if (string(argv[i]) == "-m" || string(argv[i]) == "--mensajes") {
            if (i + 1 < argc) { // Verifica que haya un siguiente argumento
                cantMensajes = stoi(argv[++i]);
            } else {
                cerr << "Error: Se esperaba un valor para -m/--mensajes." << endl;
                exit(1);
            }
        } else if (string(argv[i]) == "-n" || string(argv[i]) == "--numero") {
            if (i + 1 < argc) { // Verifica que haya un siguiente argumento
                numSensor = argv[++i];
            } else {
                cerr << "Error: Se esperaba un valor para -n/--numero." << endl;
                exit(1);
            }
        } else if (string(argv[i]) == "-i" || string(argv[i]) == "--ids") {
            if (i + 1 < argc) { // Verifica que haya un siguiente argumento
                archIds = argv[++i];
            } else {
                cerr << "Error: Se esperaba un valor para -i/--ids." << endl;
                exit(1);
            }
        }
    }
}

int main(int argc, char* argv[])
{
    string archIds;
    int cantMensajes; // Parametro cant mensajes -> Cant de mensajes que escribe el Lector en el FIFO.
    string numSensor; // Parametro numero sensor.
    int cantSegundos; // Cant de segundos que espera el FIFO entre mensajes.

    procesarParametros(argc, argv, cantMensajes, numSensor, cantSegundos, archIds);

    filesystem::path absArchPath = filesystem::absolute(archIds);

    if(!filesystem::exists(absArchPath))
    {
        cout << "El archivo enviado por parametro no existe, cuya ruta es: " << absArchPath << endl;
        exit(1);
    }

    string buffer; // Buffer en donde leo el archivo de sensores.
    string cadenaFinal; // String para juntar el numero de sensor con el ID leido del archivo.

    ofstream fifo("fifoSensores"); // Creo una variable referenciando al fifo creado para poder escribir dentro del mismo.

    ifstream archIDS(absArchPath); // Creo una variable que referencia al archivo donde tengo los ids.

    cout << "Enviando datos.. " << endl;

    while(cantMensajes > 0 && !archIDS.eof()) // Mientras tenga mensajes para enviar y no me haya quedado sin IDS para leer.
    {
        getline(archIDS, buffer); // Leo la linea.
        cadenaFinal = numSensor + "," + buffer; // Junto el numero de sensor con el ID del buffer.
        sleep(cantSegundos); // Espero la cant de segundos entre mensajes.
        fifo << cadenaFinal << endl; // Escribo en el FIFO la cadena.
        cout << "Dato enviado." << endl;
        cantMensajes--; // Le resto uno a la cant de mensajes para enviar.
    }

    fifo.close(); // Si llego aca ya escribi todo en el FIFO, lo cierro.
    
    return 0;

}