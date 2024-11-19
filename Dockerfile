# Imagen base recomendada
FROM ubuntu:24.10

# Actualizar el sistema e instalar dependencias necesarias
RUN apt-get update && \
    apt-get install -y build-essential

# Crear el directorio /apl dentro del contenedor
RUN mkdir /apl

# Copiar todo el contenido del trabajo práctico al contenedor
COPY APL2 /apl

# Establecer el directorio de trabajo
WORKDIR /apl

# Compilar todos los ejercicios
RUN make -C Ejercicio1
RUN make -C Ejercicio2
RUN make -C Ejercicio3
RUN make -C Ejercicio4
RUN make -C Ejercicio5

# Comando por defecto al iniciar el contenedor
CMD ["tail", "-f", "/dev/null"]
