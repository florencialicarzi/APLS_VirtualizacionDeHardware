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
#include <signal.h>

using namespace std;

#define NombreMemoria "memoriaPreguntados"
#define SemCliente "semCliente"
#define SemServidor "semServidor"
#define SemResponder "semResponder"
#define SemRespuesta "semRespuesta"
#define SemPartida "semPartida"
#define SemQuieroJugar "semQuieroJugar"

void procesarParametros(int argc, char* argv[], string& nickname)
{
    if(string(argv[1]) == "-h" || string(argv[1]) == "--help")
    {
        cout << "Esta es la ayuda del script, asegurese de que este ingresando todos los parametros correctamente." << "\n" <<
                 "-n | -nickname Pepe" << "\n" <<
                 "Todos estos parametros son requeridos para que el programa pueda funcionar correctamente." << endl;
        exit(0);
    }
    else if(argc < 3)
    {
        cout << "La cantidad de parametros es insuficiente, revise la ayuda con -h | --help si lo necesita." << endl;
        exit(1);
    }

    for(int i = 1; i < argc; i++)
    {
        if(string(argv[i]) == "-n" || string(argv[i]) == "--nickname")
        {
            if(i + 1 < argc)
            {
                std::string nick(argv[++i]);
                nickname = nick;
            }
            else
            {
                cerr << "Error. Se esperaba un valor para -n | --nickname." << endl;
                exit(1);
            }
        }
    }
}

int main(int argc, char* argv[])
{
    signal(SIGINT, SIG_IGN);

    size_t tam = 1024;
    size_t longitud;
    string pregunta;
    string respuesta;
    string nickname;
    int cantPreguntas;
    int i = 1;

    procesarParametros(argc, argv, nickname);

    int idMemoria = shm_open(NombreMemoria, O_CREAT | O_RDWR, 0600);

    ftruncate(idMemoria, tam);

    int* memoria = (int*) mmap(NULL, tam, PROT_READ | PROT_WRITE, MAP_SHARED, idMemoria, 0);

    close(idMemoria);

    sem_t* semCliente = sem_open(SemCliente, O_CREAT, 0600, 0);
    sem_t* semServidor = sem_open(SemServidor, O_CREAT, 0600, 0);
    sem_t* semResponder = sem_open(SemResponder, O_CREAT, 0600, 0);
    sem_t* semRespuesta = sem_open(SemRespuesta, O_CREAT, 0600, 0);
    sem_t* semPartida = sem_open(SemPartida, O_CREAT, 0600, 1);
    sem_t* semQuieroJugar = sem_open(SemQuieroJugar, O_CREAT, 0600, 0);

    if(sem_trywait(semPartida) != 0)
    {
        cout << "Ya hay una partida en curso de otro jugador." << endl;
        exit(1);
    }

    cout << "Bievenido jugador: " << nickname << endl;

    sem_post(semQuieroJugar); //*Posteo quiero jugar. -> Destrabo al servidor.
    sem_wait(semCliente); //! Hasta que el servidor no escriba, estoy bloqueado.
    cantPreguntas = *memoria;
    sem_post(semServidor);
    sem_post(semServidor); //* Destrabo el servidor.

    while(cantPreguntas > 0)
    {   
        if(sem_trywait(semCliente) == 0)
        {
            if(i == 1)
            {
                cout << "----- Pregunta  -----" << endl;
            }
            else if(i == 2)
            {
                cout << " -- Opciones -- \n" << endl;
            }

            memcpy(&longitud, memoria, sizeof(size_t));
            //cout << "Long: " << longitud << endl;
            char* buffer = new char[longitud + 1];
            memcpy(buffer, memoria + sizeof(longitud), longitud);
            buffer[longitud] = '\0';
            cout << buffer << endl;
            sem_post(semServidor);
            i++;
            delete[] buffer;
        }
        else if(sem_trywait(semResponder) == 0)
        {
            cout << "\nIngrese su respuesta (1 - 2 - 3): " << endl;;
            getline(cin, respuesta);
            cout << "Respuesta ingresada: " << respuesta << endl;
            longitud = respuesta.length();
            memcpy(memoria, &longitud, sizeof(longitud));
            memcpy(memoria + sizeof(longitud), respuesta.c_str(), longitud);
            sem_post(semRespuesta);
            i = 1;
            cantPreguntas--;
        }

    }

    sem_wait(semCliente);
    cout << "\nCant de puntos obtenidos: " << *memoria << endl;
    sem_post(semServidor);
    sem_post(semServidor); //* Destrabo el servidor.

    sem_close(semCliente);
    sem_close(semServidor);
    sem_close(semResponder);
    sem_close(semRespuesta);
    sem_close(semPartida);
    sem_unlink(SemPartida);
    munmap(memoria, tam);

    return 0;
}

// cout << "-> Opcion N" << i-1 << ": " <<buffer << endl;