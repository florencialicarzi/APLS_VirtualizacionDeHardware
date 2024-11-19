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
#include <fstream>
#include <semaphore.h>
#include <fcntl.h>

using namespace std;

void procesarParametros(int argc, char* argv[], string& nickname, int &puerto, string& direccionIP)
{
    if(string(argv[1]) == "-h" || string(argv[1]) == "--help")
    {
        cout << "Esta es la ayuda del script, asegurese de que este ingresando todos los parametros correctamente." << "\n" <<
                 "-n | -nickname Pepe / -p | --puerto 5500 / -s | --servidor 127.0.0.1" << "\n" <<
                 "Todos estos parametros son requeridos para que el programa pueda funcionar correctamente." << endl;
        exit(0);
    }
    else if(argc < 7)
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

        if(string(argv[i]) == "-s" || string(argv[i]) == "--servidor")
        {
            if(i + 1 < argc)
            {
                std::string serv(argv[++i]);
                direccionIP = serv;
            }
            else
            {
                cerr << "Error. Se esperaba un valor para -s | --servidor." << endl;
                exit(1);
            }
        }
    }
}

int main(int argc, char* argv[])
{  
    
    char buffer[2000];
    int bytesRecibidos = 0;
    string mensaje;
    string res;
    int cont = 0;
    int pos;
    int preguntas;
    bool flagErr = false;
    string nickname;
    int puerto;
    struct hostent* he;
    struct in_addr **addr_list;
    string servidor;
    

    procesarParametros(argc, argv, nickname, puerto, servidor);


    struct sockaddr_in socketConfig; //! Es una estructura que nos sirve para configurar el socket.
    memset(&socketConfig, '0', sizeof(socketConfig)); //! Seteamos la memoria en 0.
    
    if(servidor.find('.') == std::string::npos)
    {
        he = gethostbyname(servidor.c_str());
        addr_list = (struct in_addr**)he->h_addr_list;
        socketConfig.sin_addr = *addr_list[0];
    }
    else
    {
        inet_pton(AF_INET, servidor.c_str(), &socketConfig.sin_addr); //* Lo que hace este comando es traducir a binario la direccion ipv4 que nos llega por parametro.
    }

    socketConfig.sin_family = AF_INET; //* Le estamos diciendo que vamos a aceptar direcciones IPv4. (Sino seria AF_INET6).
    socketConfig.sin_port = htons(puerto); //* Puerto al cual escuchamos.

    int socketCommunication = socket(AF_INET, SOCK_STREAM, 0); //! Creamos el socket. | SOCK_STREAM indica que es TCP (orientado a conexion).

    int resultadoConexion = connect(socketCommunication, (struct sockaddr*)&socketConfig, sizeof(socketConfig)); //! Tratamos de conectarnos.

    //? Una vez que nos conectamos, vamos a intentar leer lo que el servidor nos envie.

    cout << "Bievenido jugador: " << nickname << endl;

    bytesRecibidos = read(socketCommunication, buffer, sizeof(buffer)-1);
    buffer[bytesRecibidos] = 0;
    preguntas = atoi(buffer);

    while(preguntas > 0) // Copio lo que me mande el servidor.
    {

        bytesRecibidos = read(socketCommunication, buffer, sizeof(buffer)-1);
        buffer[bytesRecibidos] = 0;

        if(bytesRecibidos == 0)
        {
            cout << "El servidor ha finalizado la conexion." << endl;
            flagErr = true;
            break;
        }

        std::string cad(buffer);

        while((pos = cad.find(',')) != std::string::npos)
        { 
            mensaje = cad.substr(0, pos);
            cad.erase(0, pos+1);

            if(cont != 1)
            {
                cout << mensaje << "\n" << endl;
            }

            cont++;
        }

        cout << cad << endl;

        cout << "Ingrese su respuesta: ";
        getline(cin, res);
        write(socketCommunication, res.c_str(), res.length());

        cont = 0;
        preguntas --;
    }

    if(!flagErr)
    {
        bytesRecibidos = read(socketCommunication, buffer, sizeof(buffer)-1);
        buffer[bytesRecibidos] = 0;
        cout << "El puntaje obtenido es: " << buffer << endl;

        bytesRecibidos = read(socketCommunication, buffer, sizeof(buffer)-1);
        buffer[bytesRecibidos] = 0;
        cout << "\n" << buffer << endl;
    }

    shutdown(socketCommunication, SHUT_RDWR);
    close(socketCommunication);

    return 0;
}