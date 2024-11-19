# Variables
CXX = g++
CXXFLAGS = -Wall -std=c++17
SERVER_SRC = Ejercicio5_Servidor.cpp
CLIENT_SRC = Ejercicio5_Cliente.cpp
SERVER_OUT = servidor
CLIENT_OUT = cliente

# Compilación de todos
all: $(SERVER_OUT) $(CLIENT_OUT)

# Compilar el servidor
$(SERVER_OUT): $(SERVER_SRC)
	$(CXX) $(CXXFLAGS) -o $(SERVER_OUT) $(SERVER_SRC)

# Compilar el cliente
$(CLIENT_OUT): $(CLIENT_SRC)
	$(CXX) $(CXXFLAGS) -o $(CLIENT_OUT) $(CLIENT_SRC)

# Limpiar archivos generados
clean:
	rm -f $(SERVER_OUT) $(CLIENT_OUT)