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
void request_file_list(int socket, char *src_dir, char *dest_dir);
void request_file(int socket, char *filename);
void inform_file_change(int socket, char *filename);

int main(int argc, char *argv[]) {
  if (argc == 2) {
    // Server code
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

    printf("Connection accepted\n");

    // Synchronization: Update directory2 with what directory1 has
    send_file_list(client_sock, argv[1]);

    close(socket_desc);
  } else if (argc == 4) {
    // Client code
    int socket_desc;
    struct sockaddr_in server;
    struct message message;

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

    // Wait to receive the list of files and update the local directory
    request_file_list(socket_desc, src_dir, dest_dir);

    // Close the socket
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
