#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <netdb.h>
#include <time.h> 

#define BUF_SIZE 1024

struct file_info {
    char name[256];
    int size;
    time_t last_updated;
};

struct message {
    char command[16];
    struct file_info file;
};

void send_file_list(int socket, char *dir);
void send_file(int socket, char *filename);
void update_file(int socket, char *filename);
void request_file_list(int socket, char *src_dir, char *dest_dir);
void request_file(int socket, char *filename);
void inform_file_change(int socket, char *filename);
void print_directory_info(char *dir); 

int main(int argc, char *argv[]) {
    if (argc == 2) {
        //Codigo Servidor
        int socket_desc, client_sock, c, read_size;
        struct sockaddr_in server, client;
        struct message message;
        char buf[BUF_SIZE];

        char *ip_server = "127.0.0.1";

        socket_desc = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_desc == -1) {
            perror("Error creating socket");
            return 1;
        }

        server.sin_family = AF_INET;
        server.sin_addr.s_addr = inet_addr(ip_server);
        server.sin_port = htons(8889);

        if (bind(socket_desc, (struct sockaddr *)&server, sizeof(server)) == -1) {
            perror("Error binding socket");
            return 1;
        }

        listen(socket_desc, 3);

        printf("Waiting for incoming connections...\n");
        c = sizeof(struct sockaddr_in);
        client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t *)&c);
        if (client_sock == -1) {
            perror("Error accepting connection");
            return 1;
        }

        print_directory_info(argv[1]);

        //Envia el src_dir al cliente
        send(client_sock, argv[1], strlen(argv[1]), 0);
        printf("Connection accepted\n");

        send_file_list(client_sock, argv[1]);

        print_directory_info(argv[1]);

        close(socket_desc);
    } else if (argc == 3) {
        //Codigo Cliente
        int socket_desc;
        struct sockaddr_in server;
        struct message message;

        char *dest_dir = argv[1];
        char *ip_server = argv[2];

        socket_desc = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_desc == -1) {
            perror("Error creating socket");
            return 1;
        }

        server.sin_family = AF_INET;
        server.sin_addr.s_addr = inet_addr(ip_server);
        server.sin_port = htons(8889);

        if (connect(socket_desc, (struct sockaddr *)&server, sizeof(server)) == -1) {
            perror("Error connecting to server");
            return 1;
        }
        char src_dir[256];
        recv(socket_desc, src_dir, sizeof(src_dir), 0);
        printf("Connected to server\n");

        print_directory_info(dest_dir);

        request_file_list(socket_desc, src_dir, dest_dir);

        print_directory_info(dest_dir);

        close(socket_desc);
    } else {
        fprintf(stderr, "Usage:\nServer: %s <directory>\nClient: %s <directory_src> <directory_dest> <ip_server>\n", argv[0], argv[0]);
        return 1;
    }

    return 0;
}

void send_file_list(int socket, char *dir) {
    DIR *dp;
    struct dirent *entry;
    struct stat st;
    char buf[BUF_SIZE];
    int i = 0;

    dp = opendir(dir);
    if (dp == NULL) {
        perror("Error opening directory");
        return;
    }

    while ((entry = readdir(dp)) != NULL) {
        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir, entry->d_name);
        stat(full_path, &st);

        if (entry->d_name[0] == '.') {
            continue;
        }

        snprintf(buf + i, sizeof(buf) - i, "%s %lld %ld\n", entry->d_name, (long long int)st.st_size, st.st_mtime);
        i += strlen(buf + i);
    }

    closedir(dp);
    send(socket, buf, i, 0);
}

int copiarArchivo(const char *nombreOriginal, const char *nombreCopia, const char *directorio) {
    FILE *archivoOriginal, *archivoCopia;
    char caracter;

    //Construye la ruta completa para el archivo original
    char rutaOriginal[100]; 
    snprintf(rutaOriginal, sizeof(rutaOriginal), "%s/%s", directorio, nombreOriginal);

    //Construye la ruta completa para el archivo de copia
    char rutaCopia[100]; 
    snprintf(rutaCopia, sizeof(rutaCopia), "%s/%s", directorio, nombreCopia);

    //Abre el archivo original en modo lectura
    archivoOriginal = fopen(rutaOriginal, "r");

    //Verifica si se pudo abrir el archivo original
    if (archivoOriginal == NULL) {
        printf("No se pudo abrir el archivo original: %s\n", rutaOriginal);
        return 1; //Termina la función con un código de error
    }

    //Abre el archivo de copia en modo escritura
    archivoCopia = fopen(rutaCopia, "w");

    //Verifica si se pudo abrir el archivo de copia
    if (archivoCopia == NULL) {
        printf("No se pudo abrir el archivo de copia: %s\n", rutaCopia);
        fclose(archivoOriginal); //Cierra el archivo original antes de terminar
        return 1; //Termina la función con un código de error
    }

    //Lee caracteres del archivo original y los escribe en el archivo de copia
    while ((caracter = fgetc(archivoOriginal)) != EOF) {
        fputc(caracter, archivoCopia);
    }

    //Cierra ambos archivos
    fclose(archivoOriginal);
    fclose(archivoCopia);

    printf("Copia creada exitosamente: %s\n", rutaCopia);

    return 0; //Termina la función con éxito
}

void request_file_list(int socket, char *src_dir, char *dest_dir) {
    char buf[BUF_SIZE];
    int bytes_received;

    bytes_received = recv(socket, buf, BUF_SIZE, 0);
    if (bytes_received <= 0) {
        perror("Error receiving file list");
        return;
    }

    char *token = strtok(buf, "\n");

    while (token != NULL) {
        char file_name[256];
        long long int file_size;
        time_t file_last_updated;

        sscanf(token, "%s %lld %ld", file_name, &file_size, &file_last_updated);
        char *file_extension = strrchr(file_name, '.');
        if (file_extension == NULL) {
            file_extension = "";
        }
        char src_file_path[1024];
        char dest_file_path[1024];
        snprintf(src_file_path, sizeof(src_file_path), "%s/%s", src_dir, file_name);
        snprintf(dest_file_path, sizeof(dest_file_path), "%s/%s", dest_dir, file_name);

        //Verifica si el archivo tiene un nombre que comienza con "dest_"
        if (strncmp(file_name, "dest_", 5) == 0) {
            //Ignora archivos "dest_" que no están en src_dir
            printf("Ignoring file: %s\n", file_name);
        } else {
            char src_file_path_copy[256];
            char dest_file_path_copy[256];
            strcpy(src_file_path_copy, src_file_path);
            strcpy(dest_file_path_copy, dest_file_path);

            struct stat src_st, dest_st;
            stat(src_file_path, &src_st);
            stat(dest_file_path, &dest_st);

            if (difftime(dest_st.st_mtime, file_last_updated) > 0) {
                printf("Conflict: %s has a newer version in %s\n", file_name, dest_dir);

                char new_name[512];
                snprintf(new_name, sizeof(new_name), "dest_%s%s", file_name, file_extension);
                snprintf(dest_file_path, sizeof(dest_file_path), "%s/%s", dest_dir, new_name);

                int count_dest = 1;
                while (access(dest_file_path, F_OK) == 0) {
                    snprintf(new_name, sizeof(new_name), "dest_%d%s%s", count_dest, file_name, file_extension);
                    snprintf(dest_file_path, sizeof(dest_file_path), "%s/%s", dest_dir, new_name);
                    count_dest++;
                }

                char new_name2[512];
                snprintf(new_name2, sizeof(new_name2), "src_%s%s", file_name, file_extension);
                snprintf(src_file_path, sizeof(src_file_path), "%s/%s", src_dir, new_name2);

                int count_src = 1;
                while (access(src_file_path, F_OK) == 0) {
                    snprintf(new_name2, sizeof(new_name2), "src_%d%s%s", count_src, file_name, file_extension);
                    snprintf(src_file_path, sizeof(src_file_path), "%s/%s", src_dir, new_name2);
                    count_src++;
                }

                //Crea una copia del archivo original en el directorio de destino con el nuevo nombre
                if (copiarArchivo(file_name, new_name, dest_dir) == 0 && copiarArchivo(file_name, new_name2, src_dir) == 0)  {
                    printf("Archivo copiado exitosamente: %s\n", file_name);
                } else {
                    printf("Error al copiar el archivo: %s\n", file_name);
                }
            } else {
                //Si no hay conflicto, actualiza el archivo
                char update_command[1024];
                snprintf(update_command, sizeof(update_command), "rsync -av %s %s", src_file_path_copy, dest_file_path_copy);
                if (system(update_command) == 0) {
                    printf("Archivo actualizado: %s\n", file_name);
                } else {
                    printf("Error al actualizar el archivo: %s\n", file_name);
                }
            }
        }

        token = strtok(NULL, "\n");
    }

    char sync_command[256];
    snprintf(sync_command, sizeof(sync_command), "rsync -av --delete --exclude='dest_*' %s/ %s/", src_dir, dest_dir);
    if (system(sync_command) == 0) {
        printf("Directorio sincronizado\n");
    } else {
        printf("Error al sincronizar el directorio\n");
    }
}

void print_directory_info(char *dir) {
    DIR *dp;
    struct dirent *entry;
    struct stat st;

    dp = opendir(dir);
    if (dp == NULL) {
        perror("Error opening directory");
        return;
    }

    printf("Directory: %s\n", dir);

    while ((entry = readdir(dp)) != NULL) {
        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir, entry->d_name);
        stat(full_path, &st);

        if (entry->d_name[0] == '.') {
            continue;
        }

        printf("File: %s, Size: %lld bytes, Last Updated: %s", entry->d_name,
               (long long int)st.st_size, ctime(&st.st_mtime));
    }

    closedir(dp);
}

void send_file(int socket, char *filename) {
  FILE *fp;
  int fd;
  char buf[BUF_SIZE];
  int bytes_read;

  fp = fopen(filename, "rb");
  if (!fp) {
    return;
  }

  fd = fileno(fp);

  send(socket, filename, strlen(filename), 0);

  while ((bytes_read = read(fd, buf, BUF_SIZE)) > 0) {
    send(socket, buf, bytes_read, 0);
  }

  fclose(fp);
}

void request_file(int socket, char *filename) {
  struct message message;
  message.command[0] = 'D';
  strcpy(message.file.name, filename);
  send(socket, &message, sizeof(message), 0);

  char buf[BUF_SIZE];
  int bytes_received;

  while ((bytes_received = recv(socket, buf, BUF_SIZE, 0)) > 0) {
    write(1, buf, bytes_received);
  }
}

void update_file(int socket, char *filename) {
  FILE *fp;
  char buf[BUF_SIZE];
  int bytes_read;

  fp = fopen(filename, "wb");
  if (!fp) {
    return;
  }

  while ((bytes_read = recv(socket, buf, BUF_SIZE, 0)) > 0) {
    fwrite(buf, 1, bytes_read, fp);
  }

  fclose(fp);
}

void inform_file_change(int socket, char *filename) {
  struct message message;
  message.command[0] = 'C';
  strcpy(message.file.name, filename);
  send(socket, &message, sizeof(message), 0);

  char buf[BUF_SIZE];
  int bytes_read;

  FILE *fp = fopen(filename, "rb");
  if (!fp) {
    perror("Error opening file");
    return;
  }

  while ((bytes_read = fread(buf, 1, BUF_SIZE, fp)) > 0) {
    send(socket, buf, bytes_read, 0);
  }

  fclose(fp);
}