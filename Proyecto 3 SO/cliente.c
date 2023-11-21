#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

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

void request_file_list(int socket, char *src_dir, char *dest_dir);
void request_file(int socket, char *filename);
void inform_file_change(int socket, char *filename);

int main(int argc, char *argv[]) {
  int socket_desc;
  struct sockaddr_in server;
  struct message message;

  if (argc != 4) {
    fprintf(stderr, "Usage: %s <directory_src> <directory_dest> <ip_server>\n", argv[0]);
    return 1;
  }

  char *src_dir = argv[1];
  char *dest_dir = argv[2];
  char *ip_server = argv[3];

  socket_desc = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_desc == -1) {
    perror("Error creating socket");
    return 1;
  }

  server.sin_family = AF_INET;
  server.sin_addr.s_addr = inet_addr(ip_server);
  server.sin_port = htons(8889);  // Use the same port as the server

  if (connect(socket_desc, (struct sockaddr *)&server, sizeof(server)) == -1) {
    perror("Error connecting to server");
    return 1;
  }

  printf("Connected to server\n");

  // Esperar a recibir la lista de archivos y actualizar el directorio local
  request_file_list(socket_desc, src_dir, dest_dir);

  // Cerrar el socket
  close(socket_desc);

  return 0;
}

void request_file_list(int socket, char *src_dir, char *dest_dir) {
  char buf[BUF_SIZE];
  int bytes_received;

  while ((bytes_received = recv(socket, buf, BUF_SIZE, 0)) > 0) {
    // Replace the hard-coded directory paths with the provided ones
    char command[256];
    snprintf(command, sizeof(command), "rsync -av --delete %s/ %s/", src_dir, dest_dir);
    system(command);
  }
}

void request_file(int socket, char *filename) {
  // Send a request for a specific file
  struct message message;
  message.command[0] = 'D';
  strcpy(message.file.name, filename);
  send(socket, &message, sizeof(message), 0);

  // Receive and process the file
  char buf[BUF_SIZE];
  int bytes_received;

  while ((bytes_received = recv(socket, buf, BUF_SIZE, 0)) > 0) {
    // Process the received file content (for example, print it)
    write(1, buf, bytes_received);
  }
}

void inform_file_change(int socket, char *filename) {
  // Inform the server about a file change
  struct message message;
  message.command[0] = 'C';
  strcpy(message.file.name, filename);
  send(socket, &message, sizeof(message), 0);

  // Send the updated file content
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
