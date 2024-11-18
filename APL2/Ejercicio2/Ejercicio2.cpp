// 43895910 Gonzalez, Luca Sebastian
// 43458509 Licarzi, Florencia Berenice
// 42597132 Gonzalez, Victor Matias
// 42364617 Polito, Thiago


#include <iostream>
#include <filesystem>
#include <fstream>
#include <thread>
#include <mutex>
#include <vector>
#include <queue>
#include <string>

namespace fs = std::filesystem;
using namespace std;

mutex output_mtx;  // Mutex para sincronizar la salida
queue<fs::path> arch_cola;  // Cola de archivos para procesar
mutex cola_mtx;  // Mutex para sincronizar el acceso a la cola


void mostrarAyuda() {
    cout << "Uso: ./Ejercicio2 -t <num_hilos> -d <directorio> -c <cadena>\n";
    cout << "Opciones:\n";
    cout << "  -t / --threads       Número de hilos a usar entero positivo.\n";
    cout << "  -d / --directorio    Directorio donde buscar archivos\n";
    cout << "  -c / --cadena        Cadena a buscar en los archivos\n";
    cout << "  -h / --help          Muestra esta ayuda\n";
}

bool parsearArgumentos(int argc, char* argv[], int& num_hilos, string& directorio, string& cad) {
    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];

        if (arg == "-t" || arg == "--threads") {
            if (i + 1 < argc) {  // Asegurarse de que hay un valor después de la bandera
                num_hilos = stoi(argv[++i]);  // Convertir a entero
            } else {
                cerr << "Error: falta el número de hilos después de " << arg << endl;
                return false;
            }
        } else if (arg == "-d" || arg == "--directorio") {
            if (i + 1 < argc) {  // Asegurarse de que hay un valor después de la bandera
                fs::path dirPath = argv[++i];  // Capturar la ruta ingresada
                // Si es una ruta relativa, convertirla en absoluta
                directorio = fs::absolute(dirPath).string();
            } else {
                cerr << "Error: falta el nombre del directorio después de " << arg << endl;
                return false;
            }
        } else if (arg == "-c" || arg == "--cadena") {
            if (i + 1 < argc) {
                cad = argv[++i];  // Tomar el valor siguiente como la cadena
            } else {
                cerr << "Error: falta la cadena después de " << arg << endl;
                return false;
            }
        } else if (arg == "-h" || arg == "--help") {
            mostrarAyuda();
            return false;  // Retorna false porque no se necesita continuar
        } else {
            cerr << "Error: opción desconocida " << arg << endl;
            mostrarAyuda();
            return false;
        }
    }

    // Verificar si todos los parámetros importantes fueron ingresados
    if (num_hilos <= 0) {
        cerr << "Error: el número de hilos debe ser mayor que cero." << endl;
        return false;
    }
    if (directorio.empty()) {
        cerr << "Error: debe especificar un directorio con la opción -d." << endl;
        return false;
    }
    if (cad.empty()) {
        cerr << "Error: debe especificar una cadena a buscar con la opción -c." << endl;
        return false;
    }

    return true;  // Todo bien
}

void buscarCadArchivo(ifstream& arch, const string& cadena, int idHilo, const fs::path& path_arch) {
    string linea;
    int numeroLinea = 0;

    // Leer el archivo línea por línea
    while (getline(arch, linea)) {
        numeroLinea++;

        // Buscar la cadena en la línea
        if (linea.find(cadena) != string::npos) {
            // Mutex para evitar que varios hilos impriman al mismo tiempo
            output_mtx.lock();  
            cout << "Hilo " << idHilo << ": " << path_arch.filename() << ": línea " << numeroLinea << endl;
            output_mtx.unlock();  
        }
    }
}

// Función que ejecutarán los hilos
void trabajoHilo(int idHilo, const string& cadena) {
    while (true) {
        // REGION CRITICA: Extraer un archivo de la cola de manera segura
        cola_mtx.lock(); //P(cola)
        if (arch_cola.empty()) {
            cola_mtx.unlock();
            return;  // Si la cola está vacía, termina el hilo
        }
        fs::path path_arch = arch_cola.front();
        arch_cola.pop();
        cola_mtx.unlock();//V(cola)

        
        ifstream arch(path_arch); // ABRO EL ARCHIVO
        if (!arch.is_open()) {
            output_mtx.lock();
            cerr << "Hilo " << idHilo << ": No se pudo abrir el archivo " << path_arch << endl;
            output_mtx.unlock();
            continue;  // Pasar al siguiente archivo si falla la apertura
        }

        // Llamar a la función que busca la cadena en el archivo
        buscarCadArchivo(arch, cadena, idHilo, path_arch);

        arch.close();// CIERRO ARCHIVO
    }
}


int main(int argc, char* argv[]) {

    int num_hilos = 0;
    string directorio;
    string cad;

    // Llamada a la función que parsea los argumentos
    if (!parsearArgumentos(argc, argv, num_hilos, directorio, cad)) {
        return 1;  // Termina si hay un error en el parsing o se solicita ayuda
    }

    if (num_hilos <= 0 || directorio.empty() || cad.empty()) {
        cerr << "Faltan argumentos necesarios o el numero de hilos es invalido." << endl;
        mostrarAyuda();
        return 1;
    }

    for (const auto& entry : fs::directory_iterator(directorio)) {
        if (entry.is_regular_file()) {
            arch_cola.push(entry.path());
        }
    }

    vector<thread> hilos;

    for (int i = 1; i <= num_hilos; ++i) {
        hilos.push_back(thread(trabajoHilo, i,cad));
    }

    // Esperar a que todos los hilos terminen (join)
    for (auto& hilo : hilos) {
        hilo.join();
    }

    return 0;
}
