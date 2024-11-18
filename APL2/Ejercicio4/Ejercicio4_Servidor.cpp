// 43895910 Gonzalez, Luca Sebastian
// 43458509 Licarzi, Florencia Berenice
// 42597132 Gonzalez, Victor Matias
// 42364617 Polito, Thiago

#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>
#include <iostream>
#include <fstream>
#include <semaphore.h>
#include <string.h>
#include <filesystem>
#include <signal.h>
#include <atomic>

using namespace std;

#define NombreMemoria "memoriaPreguntados"
#define SemServidor "semServidor"
#define SemCliente "semCliente"
#define SemResponder "semResponder"
#define SemRespuesta "semRespuesta"
#define SemServerUnico "semServerUnico"
#define SemQuieroJugar "semQuieroJugar"
#define SemPartida "semPartida"
std::ifstream archPreguntas;
atomic<bool> partidaEnProgreso(false);
int* memoria = nullptr; 
size_t tam = 1024; 

void procesarParametros(int argc, char* argv[], int& cantTotalPreguntas, string& pathArch)
{
    if(string(argv[1]) == "-h" || string(argv[1]) == "--help")
    {
        cout << "Esta es la ayuda del script, asegurese de que este ingresando todos los parametros correctamente." << "\n" <<
                 "-a | --archivo ./pepe.txt o /path1/path2/pepe.txt / -c | --cantidad 5" << "\n" <<
                 "Todos estos parametros son requeridos para que el programa pueda funcionar correctamente." << endl;
        exit(0);
    }
    else if(argc < 5)
    {
        cout << "La cantidad de parametros es insuficiente, revise la ayuda con -h | --help si lo necesita." << endl;
        exit(1);
    }

    for(int i = 1; i < argc; i++)
    {
        if(string(argv[i]) == "-a" || string(argv[i]) == "--archivo")
        {
            if(i + 1 < argc)
            {
                pathArch = argv[++i];
            }
            else
            {
                cerr << "Error. Se esperaba un valor para -a | --archivo." << endl;
                exit(1);
            }
        }

        if(string(argv[i]) == "-c" || string(argv[i]) == "--cantidad")
        {
            if(i + 1 < argc)
            {
                cantTotalPreguntas = atoi(argv[++i]);
            }
            else
            {
                cerr << "Error. Se esperaba un valor para -c | --cantidad." << endl;
                exit(1);
            }
        }

    }
}

void manejadorSenial(int senial)
{
    //! Cerrar cosas (UNLINKEAR TODOS LOS SEMAFOROS, CERRAR ARCHIVOS Y UNLINKEAR MEMORIA).
    if (senial == SIGUSR1)
    {
        if (!partidaEnProgreso.load())  // Si no hay partida en progreso
        {
            cout << "Recibida señal SIGUSR1. No hay partidas en progreso, finalizando servidor." << endl;

            // Cerrar y unlinkear semáforos
            sem_unlink(SemServidor);
            sem_unlink(SemCliente);
            sem_unlink(SemResponder);
            sem_unlink(SemRespuesta);
            sem_unlink(SemServerUnico);
            sem_unlink(SemQuieroJugar);
            sem_unlink(SemPartida);

            // Desmapear y cerrar la memoria compartida
            munmap(memoria, tam);
            shm_unlink(NombreMemoria);

            archPreguntas.close();

            exit(0);  // Finalizar el proceso servidor
        }
        else
        {
            cout << "Recibida señal SIGUSR1, pero hay una partida en progreso. No se puede finalizar." << endl;
        }
    }
}


int main(int argc, char* argv[])
{   
    signal(SIGUSR1, manejadorSenial);
    signal(SIGINT, SIG_IGN);

    string buffer;
    string respuesta;
    string cortebuffer;
    size_t longitud;
    string pathArch;
    int cantTotalPreguntas;
    int cantPreguntas;
    int cantMaxPreguntas = 0;
    unsigned cantPuntos = 0;
    int cont = 0;
    int pos;

    procesarParametros(argc, argv, cantTotalPreguntas, pathArch);
    cantPreguntas = cantTotalPreguntas;

    filesystem::path absPathArch = filesystem::absolute(pathArch);

    if(!filesystem::exists(absPathArch))
    {
        cout << "El directorio del archivo de preguntas no existe." << endl;
        exit(1);
    }

    int idMemoria = shm_open(NombreMemoria, O_CREAT | O_RDWR, 0600); // Creo o abro una referencia al espacio de memoria que quiero usar.
 
    ftruncate(idMemoria, tam); // Le asigno su tamanio.

    memoria = (int*)mmap(NULL, tam, PROT_READ | PROT_WRITE, MAP_SHARED, idMemoria, 0); // Mapeamos la memoria a nuestro proceso.

    close(idMemoria);

    sem_t* semServidor = sem_open(SemServidor, O_CREAT, 0600, 0);
    sem_t* semCliente = sem_open(SemCliente, O_CREAT, 0600, 0);
    sem_t* semRespuesta = sem_open(SemRespuesta, O_CREAT, 0600, 0); 
    sem_t* semResponder = sem_open(SemResponder, O_CREAT, 0600, 0); 
    sem_t* semServerUnico = sem_open(SemServerUnico, O_CREAT, 0600, 1);
    sem_t* semQuieroJugar = sem_open(SemQuieroJugar, O_CREAT, 0600, 0);

    if(sem_trywait(semServerUnico) != 0)
    {
        cout << "Ya existe un proceso servidor corriendo en la computadora." << endl;
        exit(1);
    }

    archPreguntas.open(absPathArch);

    while(getline(archPreguntas, buffer))
    {
        cantMaxPreguntas++;
    }

    if(cantMaxPreguntas < cantPreguntas)
    {
        cout << "La cantidad de preguntas es mayor a la maxima en el archivo, por lo tanto setearemos la maxima. " << endl;
        cantPreguntas = cantMaxPreguntas;
        cantTotalPreguntas = cantMaxPreguntas;
    }

    archPreguntas.close();
    buffer.clear();

    while(true)
    {

        sem_wait(semQuieroJugar); //! Me quedo trabado en quiero jugar.
        *memoria = cantPreguntas;
        sem_post(semCliente); //! Devuelvo un cliente V(CLIENTE).
        sem_wait(semServidor); //! Me quedo trabado aca. Como tengo 0, se traba. | Me devuelven 2 pero consumo 1.

        archPreguntas.open(absPathArch); // Creo una referencia al archivo con las preguntas.

        //! Hasta aca tengo solamente 1 sevidor.

        partidaEnProgreso.store(true);
        while(cantPreguntas > 0 && getline(archPreguntas, buffer)) // Mientras pueda leer lineas.. | Serian basicamente las rondas.
        {
            while((pos = buffer.find(',')) != std::string::npos)
            {
                cortebuffer.clear();
                sem_wait(semServidor); //! Pido el servidor. -> 1 a 0.
                cortebuffer = buffer.substr(0, pos);
                buffer.erase(0, pos+1);
            
                if(cont == 1) {
                    respuesta = cortebuffer;
                    sem_post(semServidor);
                }
                else
                {
                    longitud = cortebuffer.length();
                    //cout << "Cadena: " << cortebuffer << " Longitud: " << longitud << endl;
                    memcpy(memoria, &longitud, sizeof(longitud));
                    memcpy(memoria + sizeof(longitud), cortebuffer.c_str(), longitud + 1);
                    sleep(1);
                    sem_post(semCliente); //! Devuelvo el cliente -> 0 a 1.
                }

                cont++;
            }

            // Aca termine de procesar toda la linea excepto la ultima.

            sem_wait(semServidor);
            longitud = buffer.length();
            memcpy(memoria, &longitud, sizeof(longitud));
            memcpy(memoria + sizeof(longitud), buffer.c_str(), longitud + 1);
            sem_post(semCliente);
            sem_post(semResponder);

            // Podria poner otro semaforo para el otro, no descarto la idea en algun caso borde (para que introduzca el cliente la respuesta).

            sem_wait(semRespuesta); // Va a bloquearse porque quien responde es el cliente.
            char* res = new char[longitud + 1];
            memcpy(&longitud, memoria, sizeof(longitud));
            res[longitud] = '\0';
            memcpy(res, memoria + sizeof(longitud), longitud);

            cout << "Respuesta: " << respuesta << " Res: " << (string)res << endl;

            if(respuesta == (string)res)
            {   
                cantPuntos++;
                cout << "Cant puntos: " << cantPuntos << endl;
            }

            cont = 0;
            cantPreguntas--;
        }

        sem_wait(semServidor);
        *memoria = cantPuntos;
        sem_post(semCliente);
        sem_wait(semServidor);
        sem_wait(semServidor);

        cantPreguntas = cantTotalPreguntas;
        cantPuntos = 0;
        archPreguntas.close();
        memset(memoria, 0, tam);

        partidaEnProgreso.store(false);
    }

    //! Hacer en el signalHandler.

    sem_close(semCliente);
    sem_close(semServidor);
    sem_close(semResponder);
    sem_close(semRespuesta);
    munmap(memoria, tam);

    return 0;

}
