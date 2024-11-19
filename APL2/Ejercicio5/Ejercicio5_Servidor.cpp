// 43895910 Gonzalez, Luca Sebastian
// 43458509 Licarzi, Florencia Berenice
// 42597132 Gonzalez, Victor Matias
// 42364617 Polito, Thiago

#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <thread>
#include <fcntl.h>
#include <fstream>
#include <semaphore.h>
#include <signal.h>
#include <vector>
#include <condition_variable>
#include <filesystem>

using namespace std;

int socketEscucha;
ifstream archPreg;
int cantMaxJugadores;
int cantPreguntas;
int cantJugadores = 0;
int cantRespuestas = 0;
int cantJugadoresActuales = 0;
vector<int> vec;
mutex mtx;
condition_variable cv;

void procesarSolicitud(int numCliente, int socketCommunication, int cantPreguntas, string pathArch, vector<string> vecPreg)
{
    string buffer;
    string resMensaje;
    char buffRes[100];
    int bytesRecibidos;
    int pos;
    int comp;
    int puntosPartida = 0;
    int max = 0;
    int clienteGanador = 0;
    int cantGanadores = 0;
    bool flagErr = false;
    string pregClientes = to_string(cantPreguntas);
    string puntajeCliente;
    string puntajeGanador;
    string cGanador;
/*
    {
        unique_lock<mutex> lock(mtx);

        cantJugadores++;
        cantJugadoresActuales++;

        if(cantJugadores < cantMaxJugadores)
        {
            cv.wait(lock);
        }
        else
        {
            cv.notify_all();
        }

        lock.unlock();

    }
*/
    // Cosiderar poner un sleep(2) y un cout antes que diga "Iniciando juego.. "

    write(socketCommunication, pregClientes.c_str(), pregClientes.length());

    for(string cad : vecPreg)
    {
        //cout << cad << endl;
        write(socketCommunication, cad.c_str(), cad.length());

        bytesRecibidos = read(socketCommunication, buffRes, sizeof(buffRes-1));
        buffRes[bytesRecibidos] = 0;
        //cout << "La respuesta recibida es: " << buffRes << endl;
        if(bytesRecibidos == 0)
        {
            cout << "El cliente " << numCliente << " ha finalizado la conexion." << endl;
            flagErr = true;
            cantJugadoresActuales--;
            vec[numCliente] = 0;
            break;
        }

        pos = cad.find(',');
        cad.erase(0, pos+1); //* Borro lo primero, porque no me interesa.

        pos = cad.find(','); //* Busco la segunda ',' que tiene siempre el mensaje, guardo su pos y luego el valor del mismo.
        resMensaje = cad.substr(0, pos);

        comp = strcmp(buffRes, resMensaje.c_str());

        if(comp == 0)
        {
            //cout << "Respuesta correcta." << endl;
            puntosPartida++;
            vec[numCliente] = puntosPartida;
        }
    }

    if(!flagErr)
    {
        puntajeCliente = to_string(vec[numCliente]);
        write(socketCommunication, puntajeCliente.c_str(), puntajeCliente.length());
        
        cantRespuestas++;

        for(int i = 1; i < cantMaxJugadores; i++)
        {
            if(vec[i] > max)
            {
                max = vec[i];
                clienteGanador = i;
            }
        }

        {
            unique_lock<mutex> lock(mtx);

            if(cantRespuestas < cantJugadoresActuales)
            {
                cv.wait(lock);
            }
            else
            {
                cv.notify_all();
            }

            lock.unlock();

        }

        puntajeGanador = "El ganador es/son: ";

        for(int i = 1; i < cantMaxJugadores; i++) //! Falta ver un pedazo de logica.
        {
            if(vec[i] == max)
            {
                puntajeGanador = puntajeGanador + " - " + to_string(i);
            }
        }

        puntajeGanador = puntajeGanador + " cuyo puntaje es: " + to_string(max);    

        write(socketCommunication, puntajeGanador.c_str(), puntajeGanador.length());
    }

    shutdown(socketCommunication, SHUT_RDWR);
    close(socketCommunication);
}

void procesarParametros(int argc, char* argv[], int& cantMaxJugadores, int &puerto, string& pathArch)
{
    if(string(argv[1]) == "-h" || string(argv[1]) == "--help")
    {
        cout << "Esta es la ayuda del script, asegurese de que este ingresando todos los parametros correctamente." << "\n" <<
                 "-u | -usuarios 3 / -p | --puerto 5500 / -a | --archivo ./pepe.txt o /path1/path2/pepe.txt / -c | --cantidad 5" << "\n" <<
                 "Todos estos parametros son requeridos para que el programa pueda funcionar correctamente." << endl;
        exit(0);
    }
    else if(argc < 9)
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

        if(string(argv[i]) == "-p" || string(argv[i]) == "--puerto")
        {
            if(i + 1 < argc)
            {
                puerto = atoi(argv[++i]);
            }
            else
            {
                cerr << "Error. Se esperaba un valor para -p | --puerto." << endl;
                exit(1);
            }
        }

        if(string(argv[i]) == "-c" || string(argv[i]) == "--cantidad")
        {
            if(i + 1 < argc)
            {
                cantPreguntas = atoi(argv[++i]);
            }
            else
            {
                cerr << "Error. Se esperaba un valor para -c | --cantidad." << endl;
                exit(1);
            }
        }

        if(string(argv[i]) == "-u" || string(argv[i]) == "--usuarios")
        {
            if(i + 1 < argc)
            {
                cantMaxJugadores = atoi(argv[++i]);
            }
            else
            {
                cerr << "Error. Se esperaba un valor para -u | --usuarios." << endl;
                exit(1);
            }
        }
    }
}


int main(int argc, char* argv[])
{ 
    int puerto;
    string pathArch;
    vector<string> vecPreg;

    string bufferPreg; 
    int cantMaxPreg = 0;
    int numCliente = 0;

    procesarParametros(argc, argv, cantMaxJugadores, puerto, pathArch);
    vec.resize(cantMaxJugadores, 0);

    filesystem::path absPathArch = filesystem::absolute(pathArch); //! Transformo cualquier path que me llegue, al path absoluto.
    
    if(!filesystem::exists(absPathArch))
    {
        cout << "El directorio del archivo de preguntas no existe." << endl;
        exit(1);
    }

    struct sockaddr_in serverConfig; //! Es una variable que es una estructura que nos permite configurar el socket.
    memset(&serverConfig, '0', sizeof(serverConfig)); //* Seteamos la memoria.

    

    serverConfig.sin_family = AF_INET; //! Acepto direcciones IPv4.
    serverConfig.sin_addr.s_addr = htonl(INADDR_ANY); //! Vamos a aceptar cualquier direccion, sin embargo las podemos filtrar.
    serverConfig.sin_port = htons(puerto); //! Le decimos en que puerto vamos a escuchar.

    socketEscucha = socket(AF_INET, SOCK_STREAM, 0); //! Creamos el socket, y le decimos que es orientado a conexion.
    bind(socketEscucha, (struct sockaddr*)&serverConfig, sizeof(serverConfig)); //* Le asignamos al socket la estructura configurada anteriormente.
    //* Estamos "bindeando" nuestra estructura a nuestro socket, como "poniendole" nuestra configuracion.

    listen(socketEscucha, cantMaxJugadores); //! Lo ponemos a escuchar.

    archPreg.open(absPathArch);

    while(getline(archPreg, bufferPreg))
    {
        cantMaxPreg++;
    }

    archPreg.close();
    bufferPreg.clear();

    if(cantMaxPreg < cantPreguntas)
    {
        cout << "La cantidad de preguntas del archivo no es lo suficientemente grande asi que se seteo a la cantidad maxima." << endl;
        cantPreguntas = cantMaxPreg;
    }

    archPreg.open(absPathArch);

    for(int i = 0; i < cantPreguntas; i++)
    {
        getline(archPreg, bufferPreg);
        vecPreg.push_back(bufferPreg);
    }   

    archPreg.close();

    while(true)
    {
        int socketCommunication = accept(socketEscucha, (struct sockaddr*)NULL, NULL);
        numCliente += 1;

        thread hiloCliente(procesarSolicitud, numCliente, socketCommunication, cantPreguntas, pathArch, vecPreg);

        hiloCliente.detach();
    }   

    return 0;
}