// 43895910 Gonzalez, Luca Sebastian
// 43458509 Licarzi, Florencia Berenice
// 42597132 Gonzalez, Victor Matias
// 42364617 Polito, Thiago

#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <iostream>
#include <cstring> 

using namespace std;

void mostrar_ayuda() {
    cout << "Uso: Muestra un listado de procesos indicando su PID, y el PID de su Padre.\n";
    cout << "Opciones:\n";
    cout << "  -h, --help\t\tMuestra esta ayuda y termina.\n";
}

void manejarSIGINT(int signal)
{
    cout << "El proceso ha sido finalizado. " << endl;
}

int main(int argc, char *argv[])
{
    // Verificamos si el usuario ha ingresado el parámetro de ayuda
    if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        mostrar_ayuda();
        return 0; // Terminamos la ejecución después de mostrar la ayuda
    }

    signal(SIGINT, manejarSIGINT);

    cout << "Soy el proceso -padre- y mi PID: " << getpid() << endl;

    pid_t hijo1 = fork();

    if(hijo1 == 0) // Si entre por aca, soy el hijo 1.
    {
        // Tengo que crear 3 procesos, pero evitar que se repitan fork's para no crear procesos de mas.

        cout << "Soy el proceso -hijo1- y mi PID es: " << getpid() << " mi padre es: " << getppid() << endl;

        pid_t nieto1 = fork(); // Creo un hijo de hijo1, es decir un nieto.

        if(nieto1 == 0) // Soy el nieto1.
        {
            cout << "Soy el proceso -nieto1- y mi PID es: " << getpid() << " mi padre es: " << getppid() << endl;
        }

        else if(nieto1 > 0) // Si nieto1 > 0, soy el padre de nieto1, es decir, hijo1.
        {
            pid_t nieto3 = fork(); // Creo otro hijo, de hijo1.

            // Ahora me queda crear un proceso mas, pero tiene que ser dentro del contexto del hijo1, no del nieto3.

            if(nieto3 == 0) // Soy el nieto3.
            {
                cout << "Soy el proceso -nieto3- y mi PID es: " << getpid() << " y mi padre es: " << getppid() << endl;
            }
            
            else if(nieto3 > 0) // Si nieto3 > 0, entonces soy hijo1.
            {
                pid_t zombie = fork();

                if(zombie == 0) // Si nieto2 == 0, soy el hijo del hijo1, en este caso, el hijo nieto2.
                {
                    cout << "Soy el proceso -zombie- y mi PID es: " << getpid() << " y mi padre es: " << getppid() << endl;
                    exit(0);
                }
            }
        }

    }
    else if(hijo1 > 0) // Si entre por aca, soy el main/proceso padre.
    {
        pid_t hijo2 = fork(); // Creo el hijo 2.

        if(hijo2 == 0) // Si el hijo2 == 0, entonces soy el hijo2.
        {
            cout << "Soy el proceso -hijo2- y mi PID es: " << getpid() << " y mi padre es: " << getppid() << endl;

            pid_t demonio = fork();

            if(demonio == 0)
            {
                cout << "Soy el proceso -demonio- y mi PID es: " << getpid() << " y mi padre es: " << getppid() << endl;

                if(setsid() < 0) //creo una nueva sesión para desarraigar el proceso de la terminal en que se ejecuto
                {
                    exit(EXIT_FAILURE); //si falla que corte
                }

                chdir("/"); //cambio el directorio al raiz para evitar interferir con el directorio de ejecucion

                close(STDIN_FILENO); //evito que el usuario interactue
                close(STDOUT_FILENO); //evito que el demonio interactue
                close(STDERR_FILENO);

                while (true)
                {
                    sleep(60);
                }
            }
        }

        cout << "Presiona una tecla para terminar.. " << endl;
        cin.get();
        kill(getpid(), SIGINT);

        waitpid(hijo1, nullptr, 0);
        waitpid(hijo2, nullptr, 0);
        // waitpid(-1, &status, 0);
        // waitpid(-1, &status, 0); 
        
    }

    return 0;
}
