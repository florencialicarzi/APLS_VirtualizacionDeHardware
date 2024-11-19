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
using namespace std;

std::mutex mtx;

int respuestas_reinicio = 0;
int puntuacion_global = 0;
int juego_terminado = 0;
int juego_empezo = 0;
bool eliminar_threads = false;
int cantPreguntas;

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
        if(juego_empezo==1){
            std::cout << "Hay una partida en curso, no se puede cerrar el servidor" << std::endl;
        }else{
             sigusr1_received = true;
        }
       
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

void thread_cliente_ejecucion(int socket_cliente, int num_jugadores) {
    // El juego empieza una vez que todos los jugadores estan conectados. Una vez que empieza no se pueden conectar al servidor nuevos jugadores.
    // Este metodo llevara a cabo el manejo de los clientes conectados al servidor, llevando la cuenta de la puntuacion de cada uno y de su turno
    // Un jugador no podra jugar cuando no es su turno y solo se permite un movimiento por cliente. Al finalizar vamos a mostrar el ganador.
    // Implementamos como una mejora que el servidor le pregunte a los jugadores si quieren reiniciar el juego una vez terminado, todos los que pongan que si,
    // seran puestos en la sala de espera nuevamente. El nuevo juego comenzara una vez que todos los jugadores se encuentran en la partida
   
    char buffer[1024] = {0};
    int valread = read(socket_cliente, buffer, 1024);
    //Manejo inicial del nickname del cliente con ciertas validaciones
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
    sockets_clientes.push_back(socket_cliente);


    //Verificamos si el juego empezo, en ese casso le avisamos al jugador y lo echamos de la partida
    if (juego_empezo==1){
        std::string juego_empezado_mensaje = "Ya hay un juego en curso, porfavor conectate denuevo mas tarde \n";
        send(socket_cliente, juego_empezado_mensaje.c_str(), juego_empezado_mensaje.length(), 0);
        close(socket_cliente);
        sockets_clientes.erase(std::remove(sockets_clientes.begin(), sockets_clientes.end(), socket_cliente), sockets_clientes.end());

        
    }

    if (diccionario_puntajes.find(nickname) == diccionario_puntajes.end()) {
        diccionario_puntajes[nickname] = 0;
        cout << "Jugador " << nickname << " agregado con puntaje inicial 0." << endl;
    } else {
        cout << "El jugador " << nickname << " ya está registrado." << endl;
    }

    //agregamos al jugador a la partida
    std::string welcome_message = "Bienvenido " + nickname + "! Esperando a que se unan el resto de jugadores..." + std::to_string(sockets_clientes.size())  + "/" + std::to_string(num_jugadores) + " \n";
    send(socket_cliente, welcome_message.c_str(), welcome_message.length(), 0);
    std::string join_message = nickname + " se unio. " + std::to_string(sockets_clientes.size()) + " jugadores conectados.\n";
    broadcastMensaje(join_message, socket_cliente);

    //Agregue comandos de polling para identificar si un jugador se desconecta en la sala de espera, en ese caso se lo saca de la partida cerrando el socket y terminando con el proceso
    pollfd client_poll_fd;
    client_poll_fd.fd = socket_cliente;
    client_poll_fd.events = POLLIN | POLLERR | POLLHUP;
    while(juego_empezo == 0 && !eliminar_threads) {
        int poll_result = poll(&client_poll_fd, 1, 1000); 
        if (poll_result > 0) {
            if (client_poll_fd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
                std::unique_lock<std::mutex> lock(mtx);
                std::cout << "Cliente desconectado: " << socket_cliente << std::endl;
                broadcastMensaje("El jugador " + nickname + " se ha desconectado. Esperando el resto de jugadores..." + std::to_string(sockets_clientes.size()) + "/" + std::to_string(num_jugadores) + " \n");
                close(socket_cliente);
                sockets_clientes.erase(std::remove(sockets_clientes.begin(), sockets_clientes.end(), socket_cliente), sockets_clientes.end());
                nombres_clientes.erase(socket_cliente);
                scores_clientes.erase(socket_cliente);
                lock.unlock();
                return;
            }
            if (client_poll_fd.revents & POLLIN) {

                memset(buffer, 0, 1024);
                valread = read(socket_cliente, buffer, 1024);
                if (valread <= 0) {
                    std::unique_lock<std::mutex> lock(mtx);
                    std::cout << "Cliente desconectado: " << socket_cliente << std::endl;
                    close(socket_cliente);
                    sockets_clientes.erase(std::remove(sockets_clientes.begin(), sockets_clientes.end(), socket_cliente), sockets_clientes.end());
                    nombres_clientes.erase(socket_cliente);
                    scores_clientes.erase(socket_cliente);
                    broadcastMensaje("El jugador " + nickname + " se ha desconectado. Esperando el resto de jugadores..." + std::to_string(sockets_clientes.size()) + "/" + std::to_string(num_jugadores) + " \n");
                                        lock.unlock();
                    return;
                }
            }
        }
        //Logica para empezar el juego una vez que todos los jugadores estan conectados
            if (int(sockets_clientes.size()) == num_jugadores && sockets_clientes[indice_jugador_actual] ==  socket_cliente)  {
                broadcastMensaje("Todos los jugadores estan conectados. Empezando el juego...\n");
                juego_empezo=1;

                string bufferPreg;
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

    for(string cad : vec_preguntas)
    {
    
        send(socket_cliente, cad.c_str(), cad.length(), 0);

        bytesRecibidos = read(socket_cliente, buffRes, sizeof(buffRes-1));
        buffRes[bytesRecibidos] = 0;
        //cout << "La respuesta recibida es: " << buffRes << endl;
        if(bytesRecibidos == 0)
        {
            std::unique_lock<std::mutex> lock(mtx);
            cout << "El cliente " << nickname << " ha finalizado la conexion." << endl;
            broadcastMensaje("El jugador " + nickname + " se ha desconectado.\n", socket_cliente);
            int socket_jugador_actual=sockets_clientes[indice_jugador_actual];
            close(socket_cliente);
            sockets_clientes.erase(std::remove(sockets_clientes.begin(), sockets_clientes.end(), socket_cliente), sockets_clientes.end());
            nombres_clientes.erase(socket_cliente);
            scores_clientes.erase(socket_cliente);

            lock.unlock();
            return;
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
            diccionario_puntajes[nickname] = puntosPartida;
        }
    }
                break;
            }
    }

        
        memset(buffer, 0, 1024);
        valread = read(socket_cliente, buffer, 1024);

        //Mientras el juego haya comenzado y no se haya terminado ejecutamos la logica de juego
        if (juego_terminado == 0 && juego_empezo == 1){
            respuestas_reinicio=0;
            //Si un cliente se desconecta avisamos al resto de jugadores y cerramos el socket. Si solo queda un cliente lo damos como ganador
            if (valread <= 0) {
                std::unique_lock<std::mutex> lock(mtx);
                std::cout << "Cliente desconectado: " << socket_cliente << std::endl;
                broadcastMensaje("El jugador " + nickname + " se ha desconectado.\n", socket_cliente);
                int socket_jugador_actual=sockets_clientes[indice_jugador_actual];
                close(socket_cliente);
                sockets_clientes.erase(std::remove(sockets_clientes.begin(), sockets_clientes.end(), socket_cliente), sockets_clientes.end());
                nombres_clientes.erase(socket_cliente);
                scores_clientes.erase(socket_cliente);

                lock.unlock();
                return;
            }
        }

    
    exit(0);
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
                if(int(sockets_clientes.size())!=0){
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
                }
                else{
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
                    int new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
                    if (new_socket < 0) {
                        perror("accept");
                        exit(EXIT_FAILURE);
                    }
                    std::cout << "Nueva conexión aceptada." << std::endl;

                    pollfd client_poll_fd;
                    client_poll_fd.fd = new_socket;
                    client_poll_fd.events = POLLIN;
                    poll_fds.push_back(client_poll_fd);

                    client_threads.push_back(std::thread(thread_cliente_ejecucion, new_socket, num_jugadores));
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Excepcion: " << e.what() << std::endl;
        }
    }

    return 0;
}