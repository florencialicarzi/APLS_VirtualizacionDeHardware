/*
 >>>                 Virtualización de Hardware          <<<
 >>>                         APL 2                       <<<
 >>>                 Ejercicio 5 - Server               <<<
------           Integrantes del Grupo:                  ------
------   Alejandro Ruiz               DNI 42375350       ------
------   Luis Saini                   DNI 43744596       ------
------   Matias Alejandro Gomez       DNI 42597132       ------
------   Mariano Rodriguez Bustos     DNI 42194177       ------
------                Cuatrimeste 01-2024                ------*/


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
#define BOARD_SIZE 4
#include <atomic>
#include <chrono>
#include <thread>
#include <poll.h>
std::mutex mtx;
char tablero[BOARD_SIZE][BOARD_SIZE];
bool tablero_revelado[BOARD_SIZE][BOARD_SIZE];
int respuestas_reinicio = 0;
int puntuacion_global = 0;
int juego_terminado = 0;
int juego_empezo = 0;
bool eliminar_threads = false;
std::atomic<bool> sigusr1_received(false);
std::map<int, std::string> nombres_clientes;
std::map<int, int> scores_clientes;
std::vector<int> sockets_clientes;
std::vector<int>::size_type indice_jugador_actual = 0;
std::vector<std::thread> client_threads;
std::vector<pollfd> poll_fds;
int server_fd;


void inicializarTablero() {
//Funcion usada para crear el tablero de memotest al azar

    std::vector<char> letters;
    for (char c = 'A'; c < 'A' + BOARD_SIZE * BOARD_SIZE / 2; ++c) {
        letters.push_back(c);
        letters.push_back(c);
    }
    std::srand(unsigned(std::time(0)));
    std::shuffle(letters.begin(), letters.end(), std::default_random_engine(std::rand()));

    int index = 0;
    for (int i = 0; i < BOARD_SIZE; ++i) {
        for (int j = 0; j < BOARD_SIZE; ++j) {
            tablero[i][j] = letters[index++];
            tablero_revelado[i][j] = false;
        }
    }
}

void mostrarTablero(bool mask = false) {
//Funcion usada para mostrar el tablero en la consola del servidor, con la opcion de mascara para mostar que fichas fueron dadas vuelta
    std::cout << "Estado actual del tablero:\n";
    std::cout << "  0 1 2 3" << std::endl;
    for (int i = 0; i < BOARD_SIZE; ++i) {
        std::cout << i << " ";
        for (int j = 0; j < BOARD_SIZE; ++j) {
            if (mask && !tablero_revelado[i][j]) {
                std::cout << "*" << " ";
            } else {
                std::cout << tablero[i][j] << " ";
            }
        }
        std::cout << std::endl;
    }
}

void enviarEstadoTablero(int socket_cliente, bool mask = true) {
//Funcion usada para enviar el estado del tablero a todos los clientes conectados, con la mascara como opcion
    std::string board_state = "Estado actual del tablero:\n";
    board_state += "  0 1 2 3\n";
    for (int i = 0; i < BOARD_SIZE; ++i) {
        board_state += std::to_string(i) + " ";
        for (int j = 0; j < BOARD_SIZE; ++j) {
            if (mask && !tablero_revelado[i][j]) {
                board_state += "*";
            } else {
                board_state += tablero[i][j];
            }
            board_state += " ";
        }
        board_state += "\n";
    }
    send(socket_cliente, board_state.c_str(), board_state.length(), 0);
}

void broadcastMensaje(const std::string &message, int socket_de_origen = -1) {
//funcion usada para mandar mensaje a todos los clientes conectados, tiene la opcion de poner el socket de origen para no enviarle el mensaje a ese socket
    for (int socket : sockets_clientes) {
        if (socket != socket_de_origen) {
            send(socket, message.c_str(), message.length(), 0);
        }
    }
}

void reiniciarJuego() {
//funcion usada para reiniciar el juego una vez terminado
    inicializarTablero();
    puntuacion_global = 0;
    for (auto &pair : scores_clientes) {
        pair.second = 0;
    }
    indice_jugador_actual = 0;
}

int encontrarGanador() {
//funcion usada para encontrar el ganador una vez que todas las fichas son encontradas
    int max_score = -1;
    int winner_socket = -1;
    for (const auto &pair : scores_clientes) {
        if (pair.second > max_score) {
            max_score = pair.second;
            winner_socket = pair.first;
        }
    }
    return winner_socket;
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
                std::string mensaje_turno_resto_jugadores = "Es el turno del jugador " + nombres_clientes[sockets_clientes[indice_jugador_actual]] + "\n";
                broadcastMensaje(mensaje_turno_resto_jugadores, sockets_clientes[indice_jugador_actual]);
                enviarEstadoTablero(sockets_clientes[indice_jugador_actual], true);
                std::string mensaje_para_actual_turno = "Te toca jugar " + nombres_clientes[sockets_clientes[indice_jugador_actual]] + "\nPor favor procede a seleccionar la primera pieza que quieres levantar del tablero.\nPrimero el numero de fila y luego el numero de columna, ej: 0 1\n";
                send(sockets_clientes[indice_jugador_actual], mensaje_para_actual_turno.c_str(), mensaje_para_actual_turno.length(), 0);
                juego_empezo=1;
                break;
            }
    }
    //Aca arranca la logica central del juego
    while (!eliminar_threads) {
        
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
                //En caso de que justo era el turno del cliente que se desconecto volvemos a asignar el turno al siguiente cliente
                if(socket_jugador_actual == socket_cliente){
                    indice_jugador_actual = (indice_jugador_actual) % sockets_clientes.size();
                    std::string turn_message = "Es el turno de " + nombres_clientes[sockets_clientes[indice_jugador_actual]] + " ahora.\n";
                    std::string mensaje_para_actual_turno = "Te toca jugar " + nombres_clientes[sockets_clientes[indice_jugador_actual]] + "\nPor favor procede a seleccionar la primera pieza que quieres levantar del tablero.\nPrimero el numero de fila y luego el numero de columna, ej: 0 1\n";
                    send(sockets_clientes[indice_jugador_actual], mensaje_para_actual_turno.c_str(), mensaje_para_actual_turno.length(), 0);
                }
                std::cout << "Cantidad de clientes conectados" << sockets_clientes.size() << std::endl;
                if(sockets_clientes.size() ==1 ){
                    std::string mensaje_para_actual_turnoo = "Ganaste! Todo el resto de los jugadores se desconecto de la partida";
                    send(sockets_clientes[indice_jugador_actual], mensaje_para_actual_turnoo.c_str(), mensaje_para_actual_turnoo.length(), 0);
                    juego_terminado=1;
                    std::string termina_juego_mensaje = "Queres volver a jugar? Ingresa y para volver a jugar o cuanlquier otra tecla para desconectarte ";
                    broadcastMensaje(termina_juego_mensaje);
                }
                
                lock.unlock();
                return;
            }

            if (socket_cliente != sockets_clientes[indice_jugador_actual]) {
                std::string wait_message = "No es tu turno. Por favor espera...\n";
                send(socket_cliente, wait_message.c_str(), wait_message.length(), 0);
                continue;
            }

            std::string message(buffer);
            
            std::regex coord_regex(R"(^\d \d$)");
            if (!std::regex_match(message, coord_regex)) {
                std::string error_message = "Formato de coordenadas inválido. Debe ser 'digito espacio digito'. Ej: 1 2\n";
                send(socket_cliente, error_message.c_str(), error_message.length(), 0);
                continue;
            }
            int x1, y1;
            int x2, y2;
            std::istringstream iss(message);

            iss >> x1 >> y1;
            //Validacion de coordenadas ingresadas
            if (x1 < 0 || x1 >= BOARD_SIZE || y1 < 0 || y1 >= BOARD_SIZE || tablero_revelado[x1][y1]) {
                std::string error_message = "Coordenadas invalidas. Prueba de nuevo, revisa no elegir una ficha que ya fue levantada.\n";
                send(socket_cliente, error_message.c_str(), error_message.length(), 0);
                continue;
            }

            tablero_revelado[x1][y1] = true;
            std::string primer_movimiento_mensaje = "Levantaste la ficha: " + std::string(1, tablero[x1][y1]) + "\n";
            send(socket_cliente, primer_movimiento_mensaje.c_str(), primer_movimiento_mensaje.length(), 0);
            enviarEstadoTablero(socket_cliente); // Mostrar el estado del tablero completo al jugador actual
            std::string segundor_movimiento_mensaje = "Procede a seleccionar la segunda ficha porfavor: \n";
            send(socket_cliente, segundor_movimiento_mensaje.c_str(), segundor_movimiento_mensaje.length(), 0);

            while(true){
            memset(buffer, 0, 1024);
            valread = read(socket_cliente, buffer, 1024);
            std::string message2(buffer);
            //Aca volvemos a validar que no se haya desconectado el cliente mientras ingresa la segunda ficha, si lo hizo pasamos al siguiente turno y damos vuelta la primera ficha que levanto
            if (valread <= 0) {
                std::unique_lock<std::mutex> lock(mtx);
                std::cout << "Cliente desconectado: " << socket_cliente << std::endl;
                broadcastMensaje("El jugador " + nickname + " se ha desconectado.\n", socket_cliente);
                int socket_jugador_actual=sockets_clientes[indice_jugador_actual];
                close(socket_cliente);
                sockets_clientes.erase(std::remove(sockets_clientes.begin(), sockets_clientes.end(), socket_cliente), sockets_clientes.end());
                nombres_clientes.erase(socket_cliente);
                scores_clientes.erase(socket_cliente);
                if(socket_jugador_actual == socket_cliente){
                    indice_jugador_actual = (indice_jugador_actual) % sockets_clientes.size();
                    std::string turn_message = "Es el turno de " + nombres_clientes[sockets_clientes[indice_jugador_actual]] + " ahora.\n";
                    std::string mensaje_para_actual_turno = "Te toca jugar " + nombres_clientes[sockets_clientes[indice_jugador_actual]] + "\nPor favor procede a seleccionar la primera pieza que quieres levantar del tablero.\nPrimero el numero de fila y luego el numero de columna, ej: 0 1\n";
                    send(sockets_clientes[indice_jugador_actual], mensaje_para_actual_turno.c_str(), mensaje_para_actual_turno.length(), 0);
                    tablero_revelado[x1][y1] = false;
                }
                std::cout << "Clientes" << sockets_clientes.size() << std::endl;
                if(sockets_clientes.size() ==1 ){
                    std::string mensaje_para_actual_turnoo = "Ganaste! Todo el resto de los jugadores se desconecto de la partida";
                    send(sockets_clientes[indice_jugador_actual], mensaje_para_actual_turnoo.c_str(), mensaje_para_actual_turnoo.length(), 0);
                    juego_terminado=1;
                    std::string termina_juego_mensaje = "Queres volver a jugar? Ingresa y para volver a jugar o cuanlquier otra tecla para desconectarte ";
                    send(sockets_clientes[indice_jugador_actual], termina_juego_mensaje.c_str(), termina_juego_mensaje.length(), 0);
                }
                lock.unlock();
                return;
            }
            
            if (!std::regex_match(message2, coord_regex)) {
                std::string error_message = "Formato de coordenadas inválido. Debe ser 'digito espacio digito'. Ej: 1 2\n";
                send(socket_cliente, error_message.c_str(), error_message.length(), 0);
                
                continue;
            }
            
            std::istringstream iss2(buffer);
            iss2 >> x2 >> y2;
            //Validacion de coordenadas ingresadas, si estan mal tiene que comenzar por la primera ficha
            if (x2 < 0 || x2 >= BOARD_SIZE || y2 < 0 || y2 >= BOARD_SIZE || tablero_revelado[x2][y2]) {
                std::string error_message = "Coordenadas invalidas. Proba denuevo, revisa no elegir una ficha que ya fue levantada.\n";
                send(socket_cliente, error_message.c_str(), error_message.length(), 0);

                continue;
            }

           
            break;
            }
            tablero_revelado[x2][y2] = true;
            std::string reveal_message2 = "Levantaste la ficha: " + std::string(1, tablero[x2][y2]) + "\n";
            send(socket_cliente, reveal_message2.c_str(), reveal_message2.length(), 0);
            enviarEstadoTablero(socket_cliente);

            //Aca tenemos la logica para hacer la puntuacion del juego y validar si se han encontrado todas las fichas o no
            if (tablero[x1][y1] == tablero[x2][y2]) {
                scores_clientes[socket_cliente]++;
                puntuacion_global++;
                int total_pares = (sizeof(tablero) / sizeof(tablero[0])) * (sizeof(tablero[0]) / sizeof(tablero[0][0])) / 2;
                if (total_pares == puntuacion_global) { //El jugador levanto la ultima parehja de fichas
                    int winner_socket = encontrarGanador();
                    std::string success_message = "Haz ganado! Tu puntuacion fue de " + std::to_string(scores_clientes[winner_socket]) + "\n";
                    send(winner_socket, success_message.c_str(), success_message.length(), 0);
                    std::string match_message = nombres_clientes[winner_socket] + " ha ganado el juego con una puntuacion de: " + std::to_string(scores_clientes[winner_socket]) + "\n";
                    broadcastMensaje(match_message, winner_socket);
                    juego_terminado= 1; //Terminamos el juego para todos los clientes y ofrecemos a los clientes la posibilidad de comenzar un juego nuevo
                    std::string lose_message = "Has perdido :( \n";
                    broadcastMensaje(lose_message, winner_socket);
                    std::string termina_juego_mensaje = "Ha terminado el juego, gracias por jugar! \n  Queres volver a jugar? Ingresa y para volver a jugar o cuanlquier otra tecla para desconectarte ";
                    broadcastMensaje(termina_juego_mensaje);
                    continue;
                } else { //El jugador levanto una pareja de fichas pero no es la ultima
                    std::cout << "El jugador "<<nickname << " encontro la pareja de piezas de la letra: " << tablero[x1][y1] << std::endl;
                    mostrarTablero(true);
                    std::string success_message = "Correcto!! Tu puntuacion es: " + std::to_string(scores_clientes[socket_cliente]) + "\n";
                    send(socket_cliente, success_message.c_str(), success_message.length(), 0);
                    std::string match_message = nickname + " Encontro una pareja de piezas! Su score es: " + std::to_string(scores_clientes[socket_cliente]) + "\n";
                    broadcastMensaje(match_message, socket_cliente);
                    indice_jugador_actual = (indice_jugador_actual + 1) % sockets_clientes.size();
                }
            } else { //El jugador no levanto dos fichas iguales
                tablero_revelado[x1][y1] = false;
                tablero_revelado[x2][y2] = false;
                std::string failure_message = "Las piezas levantadas no son iguales! Mejor suerte la proxima.\n";
                send(socket_cliente, failure_message.c_str(), failure_message.length(), 0);
                indice_jugador_actual = (indice_jugador_actual + 1) % sockets_clientes.size();
                std::string turn_message = "Es el turno de " + nombres_clientes[sockets_clientes[indice_jugador_actual]] + " ahora.\n";
                broadcastMensaje(turn_message);
            }

            
            for (int socket : sockets_clientes) {
                enviarEstadoTablero(socket, true);
            }
            std::string mensaje_para_actual_turno = "Te toca jugar " + nombres_clientes[sockets_clientes[indice_jugador_actual]] + "\nPor favor procede a seleccionar la primera pieza que quieres levantar del tablero.\nPrimero el numero de fila y luego el numero de columna, ej: 0 1\n";
            send(sockets_clientes[indice_jugador_actual], mensaje_para_actual_turno.c_str(), mensaje_para_actual_turno.length(), 0);
            continue;
        }
    
        else if(juego_empezo == 0){        
                continue;
        }

        else if (juego_terminado == 1) {
            //Si termino el juego ejecutamnos toda la logica para el reinicio del servidror y del tablero

            std::string message(buffer);
            if (message != "y") {//En caso de que el jugador no quiera jugar denuevo cerramos el socket y lo sacamos de la partida
                std::unique_lock<std::mutex> lock(mtx);

                std::cout << "Cliente desconectado: " <<  socket_cliente << std::endl;
                close(socket_cliente);
                sockets_clientes.erase(std::remove(sockets_clientes.begin(), sockets_clientes.end(), socket_cliente), sockets_clientes.end());
                

                if (sockets_clientes.empty()) {
                    std::cout << "No quedan clientes conectados al servidor. El tablero fue reiniciado, nuevos jugadores pueden conectase" << std::endl;
                    std::cout << "Para cerrar el servidor por favor ingresar en otra consola kill -SIGUSR1 "<< getpid() << std::endl;
                    juego_terminado = 0;
                    juego_empezo = 0;
                    reiniciarJuego();   
                }
                lock.unlock();
                return;
            }
            else{//En caso de que el jufgador haya aceptado esperamos a que respondan el resto de los jugadores y reiniciamos la partida
                std::string reinicio_mensaje = "El juego se reiniciara en breve, estamos esperando a que tus oponentes respondan... \n";
                send(socket_cliente, reinicio_mensaje.c_str(), reinicio_mensaje.length(), 0);
                respuestas_reinicio++;
                std::string welcome_message = "Bienvenido " + nickname + "! Esperando el resto de jugadores..." + std::to_string(respuestas_reinicio) + "/" + std::to_string(num_jugadores) + " \n";
                send(socket_cliente, welcome_message.c_str(), welcome_message.length(), 0);
                while(!eliminar_threads){

                    if(respuestas_reinicio>=int(sockets_clientes.size())){
                        juego_empezo=0;
                        juego_terminado = 0;
                        reiniciarJuego();
                        indice_jugador_actual = (indice_jugador_actual) % sockets_clientes.size();
                        respuestas_reinicio=-1;

                    }
                    if (int(sockets_clientes.size()) == num_jugadores && sockets_clientes[indice_jugador_actual] ==  socket_cliente && respuestas_reinicio==-1) {
                            broadcastMensaje("Todos los jugadores estan conectados. Empezando el juego...\n");
                            std::string mensaje_turno_resto_jugadores = "Es el turno del jugador " + nombres_clientes[sockets_clientes[indice_jugador_actual]] + "\n";
                            broadcastMensaje(mensaje_turno_resto_jugadores, sockets_clientes[indice_jugador_actual]);
                            std::string mensaje_para_actual_turno = "Te toca jugar " + nombres_clientes[sockets_clientes[indice_jugador_actual]] + "\nPor favor procede a seleccionar la primera pieza que quieres levantar del tablero.\nPrimero el numero de fila y luego el numero de columna, ej: 0 1\n";
                            send(sockets_clientes[indice_jugador_actual], mensaje_para_actual_turno.c_str(), mensaje_para_actual_turno.length(), 0);
                            juego_terminado = 0;
                            juego_empezo=1;
                            break;
                    }else if(sockets_clientes[indice_jugador_actual] !=  socket_cliente && respuestas_reinicio==-1){    
                            break;
                    }
                    
                        
                }

                if(eliminar_threads){
                    exit(0);
                }
            }
            
        }
    }

    exit(0);
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

int main(int argc, char *argv[]) {
    signal(SIGINT, SIG_IGN); 
    signal(SIGUSR1, handle_signal);
    // Imprimir el PID del servidor para poder enviar la señal SIGUSR1
    std::cout << "Server PID: " << getpid() << std::endl;

    int puerto = 0;
    int num_jugadores = 0;

    int opt;
    while ((opt = getopt(argc, argv, "hp:j:")) != -1) {
        switch (opt) {
            case 'h':
                std::cout << "Ejemplo de uso: " << argv[0] << " -p <puerto> -j <num_jugadores>" << std::endl;
                std::cout << "Opciones:" << std::endl;
                std::cout << "  -h                    Muestra este mensaje de ayuda" << std::endl;
                std::cout << "  -p <puerto>           Especifica el puerto que se usara" << std::endl;
                std::cout << "  -j <num_jugadores>    Especifica el número de jugadores" << std::endl;
                exit(EXIT_SUCCESS);
            case 'p':
                puerto = std::stoi(optarg);
                break;
            case 'j':
                num_jugadores = std::stoi(optarg);
                break;
            default:
                std::cerr << "Ejemplo de uso: " << argv[0] << " -p <puerto> -j <num_jugadores>" << std::endl;
                exit(EXIT_FAILURE);
        }
    }

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
    inicializarTablero();
    mostrarTablero();

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