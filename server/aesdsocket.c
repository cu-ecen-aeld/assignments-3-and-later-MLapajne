#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netdb.h>
#include <syslog.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <libgen.h>
#include <signal.h>
#include <unistd.h>
#include <arpa/inet.h>

extern int errno;



#define PORT "9000"  // the port users will be connecting to
#define BACKLOG 10   // how many pending connections queue will hold
#define FILE_PATH "/var/tmp/aesdsocketdata"

int sockfd1 = 0;
int sockfd2 = 0;
bool daemon_mode = false;




// Signal handler function
void handle_signal(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        syslog(LOG_INFO, "Received SIGINT or SIGTERM. Closing the server.\n");
        shutdown(sockfd1, SHUT_RDWR);
        remove(FILE_PATH);
        closelog();
        exit(EXIT_SUCCESS);
    }
}

// Function to create directories if they don't exist
int createDirectories(const char *dir_path) {
    char dir_path_copy[strlen(dir_path) + 1];
    strcpy(dir_path_copy, dir_path);

    const char *dir_name = dirname(dir_path_copy);
    DIR *dir = opendir(dir_name);
    if (dir == NULL) {
        int status = mkdir(dir_name, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH); 
        if (status != 0) {
            perror("Error creating directories");
            syslog(LOG_ERR, "Error creating directories: %s", dir_name);
            return EXIT_FAILURE;
        }
    } else {
        closedir(dir);
    }

    return EXIT_SUCCESS;
}

// Function to write content to a file
int writeFile(const char *file_path, const char *content, ssize_t bytes_received) {
    FILE *file = fopen(file_path, "a");
    if (!file) {
        perror("Error creating file");
        syslog(LOG_ERR, "Error creating file: %s \n", file_path);
        return EXIT_FAILURE;
    } else {
        //fputs(content, file);
        fwrite(content, 1, bytes_received, file);
        fclose(file);
    }

    syslog(LOG_DEBUG, "Writing %s to %s", content, file_path);

    return EXIT_SUCCESS;
}

void handle_connection() {
    struct sockaddr_in client_addr = {0};
    socklen_t client_addr_len = sizeof(client_addr);
    ssize_t data_recv = 0;
	char buffer[1024] = {0};
    FILE *data_file = NULL;

    if (getpeername(sockfd2, (struct sockaddr *)&client_addr, &client_addr_len) == 0) {
            syslog(LOG_INFO, "Accepted connection from %s", inet_ntoa(client_addr.sin_addr));
    }

	while(1) {
		
        if ((sockfd2 = accept(sockfd1, (struct sockaddr *)&client_addr, &client_addr_len)) == -1) {
			perror("Error on accept() call \n");
			exit(EXIT_FAILURE);
		}
        do {
            data_recv = recv(sockfd2, buffer, sizeof buffer, 0);
            if (data_recv < 0) {
                perror("Connection closed or error while receiving\n");
                break;
            } else if (data_recv > 0) {
    
                //printf("Data received: %ld : %s \n", data_recv, buffer);

                if (writeFile(FILE_PATH, buffer, data_recv) == EXIT_FAILURE) {
                    exit(EXIT_FAILURE);
                }
                
                if(strstr(buffer, "\n") != NULL) {
                    // Send the content of the data file back to the client
                    data_file = fopen(FILE_PATH, "r");
                    if (data_file == NULL) {
                        perror("Error opening data file");
                        exit(-1);
                    }
                    char *line = NULL;
                    size_t length = 0;
                    ssize_t bytes_read = 0;
                    while ((bytes_read = getline(&line, &length, data_file)) > 0) {
                        if (send(sockfd2, line, bytes_read, 0) == -1) {
                            perror("Error sending data");
                            break;
                        }
                    }
                    free(line);
                    fclose(data_file);
                }
            }
        } while(data_recv > 0);

	    close(sockfd2);
        syslog(LOG_DEBUG, "Closed conection from %s", inet_ntoa(client_addr.sin_addr));
	}
}





int main(int argc, char *argv[]) {

    int status;
    struct addrinfo *servinfo, hints; //kaže na rezultate


    openlog(NULL, 0, LOG_USER);

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    if (createDirectories(FILE_PATH) == EXIT_FAILURE) {
        exit(EXIT_FAILURE);
    }


    if (argc > 1 && strcmp(argv[1], "-d") == 0) {
        daemon_mode = true;
        //printf("Daemon mode!\n");
    }

    memset(&hints, 0, sizeof hints); //struct mora biti prazen
    hints.ai_family = AF_UNSPEC; //ni vežna ali je IPv4 ali 6
    hints.ai_socktype = SOCK_STREAM; //TCP tok
    hints.ai_flags = AI_PASSIVE; //vstavi katerikoli IP za mene


    if ((status = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        perror("getaddrinfo error\n");
        exit(EXIT_FAILURE);
    }


    if ((sockfd1 = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol)) == -1 ) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    //socket option to reuse the address
    int reuse = 1;
    if (setsockopt(sockfd1, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1) {
        perror("setsockopt");
        close(sockfd1);
        exit(EXIT_FAILURE);
    }

    if (bind(sockfd1, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
        perror("bind failed");
        close(sockfd1);
        exit(EXIT_FAILURE);
    }


    freeaddrinfo(servinfo);
    
    if(listen(sockfd1, BACKLOG) == -1) {
		perror("Error on listen call \n");
        close(sockfd1);
        exit(EXIT_FAILURE);
	}

    if (daemon_mode) {
        if (fork() == 0) {
            handle_connection();
            exit(0);
        } else {
            exit(0);
        }
    } else {
        handle_connection();
    }
    
    
    close(sockfd1);
    closelog();

    return 0;
}