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
#include <string>
#include <signal.h>
#include <filesystem>

using namespace std;

std::ofstream archLog;
std::ifstream idArchVal;
std::ifstream fifo;

void endemoniar() 
{
    pid_t pid = fork();
    if (pid < 0) {
        perror("Fork failed");
        exit(1);
    }
    if (pid > 0) {
        exit(0); // Termina el proceso padre
    }

    // Crear una nueva sesión
    if (setsid() < 0) {
        perror("setsid");
        exit(1);
    }

    // Cambiar el directorio de trabajo
    // if (chdir("/") < 0) {
    //     perror("chdir");
    //     exit(1);
    // }

    // Redirigir entrada/salida
    int null_fd = open("/dev/null", O_RDWR);
    dup(null_fd); // STDIN
    dup(null_fd); // STDOUT
    dup(null_fd); // STDERR
    close(null_fd);
}

void procesarParametros(int argc, char* argv[], string& pathArch)
{
    if(string(argv[1]) == "-h" || string(argv[1]) == "--help")
    {
        cout << "Esta es la ayuda del script, asegurese de que este ingresando todos los parametros correctamente." << "\n" <<
                 "-l | --log ./pepe.txt o /path1/path2/pepe.txt" << "\n" <<
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
        if(string(argv[i]) == "-l" || string(argv[i]) == "--log")
        {
            if(i + 1 < argc)
            {
                pathArch = argv[++i];
            }
            else
            {
                cerr << "Error. Se esperaba un valor para -l | --log." << endl;
                exit(1);
            }
        }
    }
}

void manejadorSenial(int senial)
{
    cout << "Proceso demonio finalizado." << endl;

    archLog.close();
    idArchVal.close();
    fifo.close();

    exit(0);
}

int main(int argc, char* argv[])
{  
    endemoniar();

    signal(SIGTERM, manejadorSenial);

    string buffer; // Buffer para la cadena del FIFO (numSensor + ID).
    string bufferIds; // Buffer para leer la cadena de IDS validados.
    int pos; // Variable para parsear la cadena leida del FIFO.
    string pathArch;

    string numSensor; 
    string ID;
    char hora[2000];
    string cadenaLog;

    procesarParametros(argc, argv, pathArch);

    filesystem::path absArchPath = filesystem::absolute(pathArch);

    mkfifo("fifoSensores", 0666); // Creo un fifo para los sensores, en modo de lectura y escritura para todos.

    archLog.open(absArchPath, ios::app);

    while(true)
    {
        fifo.open("fifoSensores"); // Variable referenciada al archivo de fifoSensores para leer.

        while(getline(fifo, buffer)) // Mientras tenga lineas en el FIFO..
        {
            // Separo la cadena leida en numero de sensor y id leido.
            pos = buffer.find(',');
            numSensor = buffer.substr(0, pos);
            ID = buffer.substr(pos + 1);

            // Por cada ID leido, tengo que buscar en todo el archivo de idsValidos, si se encuentra en el mismo.

            idArchVal.open("idsValidos.txt");

            while(getline(idArchVal, bufferIds)) // Mientras tenga IDS validos, los voy comparando.
            {
                time_t ticks = time(NULL);
                snprintf(hora, sizeof(hora), "%.24s", ctime(&ticks));


                if(bufferIds == ID) // Si coinciden, el ID sensoriado es valido, y lo escribo en el log.
                {
                    cadenaLog = "Sensado el: " + (string)hora + " por el sensor: " + numSensor + " ID validado: " + ID;
                    break;
                }
                else
                {
                cadenaLog = "Sensado el: " + (string)hora + " por el sensor: " + numSensor + " NO se pudo validar el ID: " + ID;
                }
            }

            archLog << cadenaLog << endl; 
            idArchVal.close();
        }

        fifo.clear(); // Limpio el FIFO.
        fifo.close();
        sleep(1);
    }

    return 0;
}