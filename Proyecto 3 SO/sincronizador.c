#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <dirent.h>

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

// Function declarations
void send_file_list(int socket, char *dir);
void send_file(int socket, char *filename);
void update_file(int socket, char *filename);

int main(int argc, char *argv[]) {
  int socket_desc, client_sock, c, read_size;
  struct sockaddr_in server, client;
  struct message message;
  char buf[BUF_SIZE];

  if (argc < 2 || argc > 3) {
    fprintf(stderr, "Usage: %s <directory> [ip_server]\n", argv[0]);
    return 1;
  }

  char *ip_server = argv[2] ? argv[2] : "127.0.0.1";

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

  printf("Connection accepted\n");

  // Sincronización: Actualizar directorio2 con lo que tiene directorio1
  send_file_list(client_sock, argv[1]);

  close(socket_desc);

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
    char full_path[256];
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
