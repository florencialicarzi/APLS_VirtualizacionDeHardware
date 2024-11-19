/*
 >>>                 Virtualización de Hardware          <<<
 >>>                         APL 2                       <<<
 >>>                 Ejercicio 5 - Cliente               <<<
------           Integrantes del Grupo:                  ------
------   Alejandro Ruiz               DNI 42375350       ------
------   Luis Saini                   DNI 43744596       ------
------   Matias Alejandro Gomez       DNI 42597132       ------
------   Mariano Rodriguez Bustos     DNI 42194177       ------
------                Cuatrimeste 01-2024                ------*/

#include <iostream>
#include <string>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/select.h>


int main(int argc, char *argv[]) {
    std::string nickname;
    std::string ip_servidor;
    int puerto = 0;

    int opt;
    while ((opt = getopt(argc, argv, "hn:s:p:")) != -1) {
        switch (opt) {
            case 'h':
                std::cerr << "Modo de uso: " << argv[0] << " -n <nickname> -s <ip_servidor> -p <puerto>" << std::endl;
                std::cout << "Opciones:" << std::endl;
                std::cout << "  -n <nickname>         Nombre de usuario que usara el jugador" << std::endl;
                std::cout << "  -s <ip_servidor>      Ip del servidor a conectarse" << std::endl;
                std::cout << "  -p <puerto>           Puerto del servidor a conectarse" << std::endl;
                exit(EXIT_SUCCESS);
            case 'n':
                nickname = optarg;
                break;
            case 's':
                ip_servidor = optarg;
                break;
            case 'p':
                puerto = std::stoi(optarg);
                break;
            default:
                std::cerr << "Modo de uso: " << argv[0] << " -n <nickname> -s <ip_servidor> -p <puerto>" << std::endl;
                exit(EXIT_FAILURE);
        }
    }

    if (nickname.empty() || ip_servidor.empty() || puerto == 0) {
        std::cerr << "Todos los parametros deben ser especificados (nickname, IP Servidor y puerto)" << std::endl;
        std::cerr << "Modo de uso: " << argv[0] << " -n <nickname> -s <ip_servidor> -p <puerto>" << std::endl;
        exit(EXIT_FAILURE);
    }

    int sock = 0, valread;
    struct sockaddr_in serv_addr;
    char buffer[1024] = {0};
    
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cout << "Error en la creacion del socket" << std::endl;
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(puerto);

    if (inet_pton(AF_INET, ip_servidor.c_str(), &serv_addr.sin_addr) <= 0) {
        std::cout << "Direccion ip invalida" << std::endl;
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cout << "La conexion ha fallado, no ha sido posible conectarlo con el servidor" << std::endl;
        return -1;
    }

    send(sock, nickname.c_str(), nickname.length(), 0);

    fd_set readfds;

    while (true) {
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        FD_SET(STDIN_FILENO, &readfds);

        int max_sd = sock > STDIN_FILENO ? sock : STDIN_FILENO;

        int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if (activity < 0 && errno != EINTR) {
            std::cout << "Error en select" << std::endl;
        }

        if (FD_ISSET(sock, &readfds)) {
            memset(buffer, 0, 1024);
            valread = read(sock, buffer, 1024);
            if (valread <= 0) {
                std::cout << "Se ha desconectado del servidor" << std::endl;
                break;
            }
            std::cout << buffer << std::endl;
        }

        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            std::string input;
            std::getline(std::cin, input);
            send(sock, input.c_str(), input.length(), 0);
        }
    }

    close(sock);
    return 0;
}