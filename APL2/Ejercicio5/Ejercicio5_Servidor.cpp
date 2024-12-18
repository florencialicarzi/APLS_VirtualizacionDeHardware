#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <string>
#include <map>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <getopt.h>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <sstream>
#include <random>
#include <csignal>
#include <regex>
#include <atomic>
#include <chrono>
#include <thread>
#include <poll.h>
#include <iostream>
#include <fstream> // Asegúrate de incluir este encabezado
#include <vector>
#include <string>
#include <sstream>
#include <unordered_map> 
using namespace std;

std::mutex mtx;

int respuestas_reinicio = 0;
int puntuacion_global = 0;
int juego_terminado = 0;
int juego_empezo = 0;
bool eliminar_threads = false;
int cantPreguntas;
std::atomic<int> cantidadDeJugadoresActuales{0};
std::atomic<int> cantJugadoresPartidaFinalizada{0};


std::atomic<bool> sigusr1_received(false);
std::map<int, std::string> nombres_clientes;
std::map<int, int> scores_clientes;
std::vector<int> sockets_clientes;
std::vector<int>::size_type indice_jugador_actual = 0;
std::vector<std::thread> client_threads;
std::vector<pollfd> poll_fds;
int server_fd;

vector<string> vec_preguntas;
unordered_map<string, int> diccionario_puntajes;


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


void cargarPreguntas(const string& nombreArchivo) {
    ifstream archivo(nombreArchivo);

    if (!archivo.is_open()) {
        cerr << "Error: No se pudo abrir el archivo " << nombreArchivo << endl; // Devuelve un vector vacío si el archivo no se puede abrir
    }

    string linea;
    while (getline(archivo, linea)) {
        vec_preguntas.push_back(linea); // Almacena cada línea en el vector
    }
    archivo.close();

}

void handle_signal(int sig) {
    if (sig == SIGUSR1) {
        std::cout << "Señal SIGUSR1 recibida." << std::endl;
        std::cout << "Finalizacion abrupta del servidor. Desconectando clientes..." << std::endl;
        sigusr1_received = true;
    }

    if (sigusr1_received) {
    std::cout << "Señal SIGUSR1 recibida. Finalizando el servidor..." << std::endl;

    eliminar_threads = true;
    std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Esperar un momento

    // Cerrar todos los sockets de cliente
    for (int socket : sockets_clientes) {
        send(socket, "El servidor se está cerrando.\n", 30, 0);
        close(socket);
        std::cout << "Cerrando socket cliente: " << socket << std::endl;
    }

    // Esperar a que los threads terminen
    for (std::thread &t : client_threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    // Limpiar estructuras
    sockets_clientes.clear();
    client_threads.clear();
    close(server_fd);

    std::cout << "Servidor cerrado correctamente.\n";
    std::exit(0); // Salir sin lanzar excepciones
}

}


void broadcastMensaje(const std::string &message, int socket_de_origen = -1) {
//funcion usada para mandar mensaje a todos los clientes conectados, tiene la opcion de poner el socket de origen para no enviarle el mensaje a ese socket
    for (int socket : sockets_clientes) {
        if (socket != socket_de_origen) {
            send(socket, message.c_str(), message.length(), 0);
        }
    }
}

// Función para formatear la línea del archivo
std::string formatearPregunta(const std::string& linea) {
    // Dividir la línea por comas
    size_t pos = linea.find(',');
    std::string pregunta = linea.substr(0, pos);
    std::string resto = linea.substr(pos + 1);

    // Obtener las opciones
    std::vector<std::string> opciones;
    while ((pos = resto.find(',')) != std::string::npos) {
        opciones.push_back(resto.substr(0, pos));
        resto.erase(0, pos + 1);
    }
    opciones.push_back(resto); // Última opción

    // Construir la pregunta formateada
    std::string pregunta_formateada = pregunta + "\n";
    for (size_t i = 1; i < opciones.size(); ++i) {
        pregunta_formateada += std::to_string(i) + " - " + opciones[i] + "\n";
    }

    return pregunta_formateada;
}

void thread_cliente_ejecucion(int socket_cliente, int num_jugadores) {
    char buffer[1024] = {0};
    int valread = read(socket_cliente, buffer, 1024);

    // Manejo inicial del nickname del cliente
    if (valread <= 0) {
        std::cerr << "Error al leer el nombre del cliente o el cliente se desconectó antes de enviar el nombre." << std::endl;
        close(socket_cliente);
        return;
    }

    std::string nickname(buffer);
    nickname.erase(std::remove(nickname.begin(), nickname.end(), '\n'), nickname.end());
    nickname.erase(std::remove(nickname.begin(), nickname.end(), '\r'), nickname.end());

    if (nickname.empty()) {
        std::cerr << "Nombre de cliente vacío recibido. Desconectando cliente." << std::endl;
        close(socket_cliente);
        return;
    }

    nombres_clientes[socket_cliente] = nickname;
    scores_clientes[socket_cliente] = 0;

    {
        std::lock_guard<std::mutex> lock(mtx);
        sockets_clientes.push_back(socket_cliente);
        cantidadDeJugadoresActuales++;
    }

    // Verificar si el juego ya empezó
    if (juego_empezo == 1) {
        std::string mensaje = "Ya hay un juego en curso. Conéctate más tarde.\n";
        send(socket_cliente, mensaje.c_str(), mensaje.length(), 0);
        close(socket_cliente);
        {
            std::lock_guard<std::mutex> lock(mtx);
            sockets_clientes.erase(std::remove(sockets_clientes.begin(), sockets_clientes.end(), socket_cliente), sockets_clientes.end());
            cantidadDeJugadoresActuales--;
        }
        return;
    }

    // Agregar al jugador al diccionario de puntajes
    if (diccionario_puntajes.find(nickname) == diccionario_puntajes.end()) {
        diccionario_puntajes[nickname] = 0;
    }

    // Mensaje de bienvenida
    std::string mensaje_bienvenida = "Bienvenido " + nickname + "! Esperando a que se unan el resto de jugadores...\n";
    send(socket_cliente, mensaje_bienvenida.c_str(), mensaje_bienvenida.length(), 0);

    // Esperar a que todos los jugadores se conecten
    while (juego_empezo == 0 && !eliminar_threads) {
        if (cantidadDeJugadoresActuales == num_jugadores) {
            juego_empezo = 1;
            broadcastMensaje("Todos los jugadores están conectados. Empezando el juego...\n");
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    // Lógica de preguntas
    int preguntas_respondidas = 0;
    int total_preguntas = cantPreguntas;
    while (preguntas_respondidas < total_preguntas) {
        const std::string& pregunta_cruda = vec_preguntas[preguntas_respondidas % vec_preguntas.size()];
        std::string pregunta_formateada = formatearPregunta(pregunta_cruda);

        // Enviar la pregunta formateada al jugador actual
        send(socket_cliente, pregunta_formateada.c_str(), pregunta_formateada.length(), 0);

        // Esperar respuesta del jugador
        memset(buffer, 0, sizeof(buffer));
        int bytes_leidos = read(socket_cliente, buffer, sizeof(buffer));
        if (bytes_leidos <= 0) {
            // Manejo de desconexión
            std::cerr << "El jugador " << nickname << " se desconectó.\n";
            broadcastMensaje("El jugador " + nickname + " se ha desconectado.\n", socket_cliente);

            // Actualizar estructuras y finalizar para este jugador
            {
                std::lock_guard<std::mutex> lock(mtx);
                close(socket_cliente);
                sockets_clientes.erase(std::remove(sockets_clientes.begin(), sockets_clientes.end(), socket_cliente), sockets_clientes.end());
                nombres_clientes.erase(socket_cliente);
                scores_clientes.erase(socket_cliente);
                cantidadDeJugadoresActuales--;

                if(cantidadDeJugadoresActuales == 0)
                {
                     // Limpiar estructuras y variables globales
                    {
                        juego_empezo = 0;
                         std::cout << "Todos los jugadores se desconectaron. Reiniciando el servidor para nuevas conexiones.\n";
                    }

                }
            }

            return; // Salir del bucle para este jugador
        }

        buffer[bytes_leidos] = '\0'; // Asegurar fin de cadena
        std::string respuesta(buffer);

        // Verificar respuesta
        size_t pos = pregunta_cruda.find(',');
        std::string respuesta_correcta = pregunta_cruda.substr(pos + 1, pregunta_cruda.find(',', pos + 1) - pos - 1);
        if (respuesta == respuesta_correcta) {
            scores_clientes[socket_cliente]++;
            diccionario_puntajes[nickname]++;
            send(socket_cliente, "Respuesta correcta!\n", 20, 0);
        } else {
            send(socket_cliente, "Respuesta incorrecta.\n", 22, 0);
        }

        preguntas_respondidas++;
    }

    // Notificar al servidor que este jugador terminó sus preguntas
    {
        std::lock_guard<std::mutex> lock(mtx);
        cantJugadoresPartidaFinalizada++;
    }

    // Finalizar el juego si todos los jugadores han terminado o se han desconectado
    if (cantidadDeJugadoresActuales == cantJugadoresPartidaFinalizada) {
        broadcastMensaje("El juego ha terminado. Aquí están los resultados:\n");
        for (const auto& [nickname, puntaje] : diccionario_puntajes) {
            std::string mensaje_puntaje = nickname + ": " + std::to_string(puntaje) + "\n";
            broadcastMensaje(mensaje_puntaje);
        }

        // Cerrar las conexiones de todos los jugadores restantes
        for (int socket : sockets_clientes) {
            send(socket, "La partida ha terminado. Cerrando conexión...\n", 45, 0);
            close(socket);
        }

        // Limpiar estructuras y variables globales
        {
            std::lock_guard<std::mutex> lock(mtx);
            sockets_clientes.clear();
            nombres_clientes.clear();
            scores_clientes.clear();
            diccionario_puntajes.clear();
            cantidadDeJugadoresActuales = 0;
            cantJugadoresPartidaFinalizada = 0;
            juego_empezo = 0;
        }

        std::cout << "Partida finalizada. Servidor listo para nuevas conexiones.\n";
    }
}




int main(int argc, char *argv[]) {
    signal(SIGINT, SIG_IGN); 
    signal(SIGUSR1, handle_signal);
    // Imprimir el PID del servidor para poder enviar la señal SIGUSR1
    std::cout << "Server PID: " << getpid() << std::endl;

    string pathArch;
    int puerto = 0;
    int num_jugadores = 0;

    procesarParametros(argc, argv, num_jugadores, puerto, pathArch);

    if (puerto == 0 || num_jugadores == 0) {
        std::cerr << "Se tiene que especificar tanto el puerto como la cantidad de jugadores" << std::endl;
        std::cerr << "Ejemplo de ejecucion: " << argv[0] << " -p <puerto> -j <num_jugadores>" << std::endl;
        exit(EXIT_FAILURE);
    }

    // Creamos el socket
    int server_fd;
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Fallo la creacion del socket");
        exit(EXIT_FAILURE);
    }

    // Configuramos la dirección y el puerto del socket
    struct sockaddr_in address;
    address.sin_family = AF_INET; // Usar IPv4
    address.sin_addr.s_addr = INADDR_ANY; // Aceptar conexiones desde cualquier interfaz de red
    address.sin_port = htons(puerto); // Configurar el puerto
    int addrlen = sizeof(address);

    // Asignamos el socket a la dirección y el puerto especificados
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Ya hay otro servidor creado en este puerto");
        exit(EXIT_FAILURE);
    }

    // Ponemos el socket en modo de escucha para aceptar conexiones entrantes
    if (listen(server_fd, num_jugadores) < 0) {
        perror("Error al poner al socket en modo escucha");
        exit(EXIT_FAILURE);
    }

    pollfd server_poll_fd;
    server_poll_fd.fd = server_fd;
    server_poll_fd.events = POLLIN;
    poll_fds.push_back(server_poll_fd);
    std::cout << "Servidor iniciado en la IP: " << inet_ntoa(address.sin_addr) << " en el puerto: " << puerto << std::endl;
    std::cout << "Esperando que se conecten " << num_jugadores << " jugadores..." << std::endl;
    
    cargarPreguntas(pathArch);

    std::vector<std::thread> client_threads;

    while (true) {
    try {
        if (sigusr1_received) {
            if (int(sockets_clientes.size()) != 0) {
                std::cout << "Señal recibida para finalizar el servidor." << std::endl;
                eliminar_threads = true;
                std::this_thread::sleep_for(std::chrono::seconds(1));
                // Cerrar todos los sockets de cliente
                for (int socket : sockets_clientes) {
                    send(socket, "El servidor se está cerrando.\n", 30, 0);
                    close(socket);
                    std::cout << "Cerrando socket cliente: " << socket << std::endl;
                }
                eliminar_threads = true;
                std::this_thread::sleep_for(std::chrono::seconds(1));
                sockets_clientes.clear();
                client_threads.clear();
                close(server_fd);
                std::cout << "Servidor cerrado." << std::endl;
                exit(0);
            } else {
                close(server_fd);
                std::cout << "Servidor cerrado." << std::endl;
                exit(0);
            }
        }

        int activity = poll(poll_fds.data(), poll_fds.size(), 2000); // Timeout de 1 segundo

        if (activity < 0 && errno != EINTR) {
            std::cerr << "Error en poll" << std::endl;
            break;
        }

        if (activity > 0) {
            if (poll_fds[0].revents & POLLIN) {
                int new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
                if (new_socket < 0) {
                    perror("accept");
                    exit(EXIT_FAILURE);
                }
                std::cout << "Nueva conexión aceptada.\n";

                pollfd client_poll_fd;
                client_poll_fd.fd = new_socket;
                client_poll_fd.events = POLLIN;
                poll_fds.push_back(client_poll_fd);

                client_threads.push_back(std::thread(thread_cliente_ejecucion, new_socket, num_jugadores));
            }
        }

        // Verificar si todos los jugadores se desconectaron
        {
            std::lock_guard<std::mutex> lock(mtx);
            if (cantidadDeJugadoresActuales == 0 && juego_empezo == 0) {

                // Resetear variables globales
                sockets_clientes.clear();
                nombres_clientes.clear();
                scores_clientes.clear();
                diccionario_puntajes.clear();
                cantJugadoresPartidaFinalizada = 0;
                juego_empezo = 0;
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Excepción: " << e.what() << std::endl;
    }
}


    return 0;
}