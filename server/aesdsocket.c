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
#include <sys/queue.h>
#include <pthread.h>

extern int errno;



#define PORT "9000"  // the port users will be connecting to
#define BACKLOG 10   // how many pending connections queue will hold
#define FILE_PATH "/var/tmp/aesdsocketdata"
#define TIMESTAMP_INTERVAL 10

int sockfd1 = 0;
//int sockfd2 = 0;
bool daemon_mode = false;
pthread_mutex_t mutex;




// SLIST.
typedef struct slist_data_s slist_data_t;
struct slist_data_s {
    pthread_t thread;
    int client_socket;
    SLIST_ENTRY(slist_data_s) entries;
};

//slist_data_t *datap=NULL;
SLIST_HEAD(slisthead, slist_data_s) head;



// Signal handler function
void handle_signal(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        syslog(LOG_INFO, "Received SIGINT or SIGTERM. Closing the server.\n");


        slist_data_t *datap_iter=NULL;
        SLIST_FOREACH(datap_iter, &head, entries) {
            if (pthread_cancel(datap_iter->thread) == -1) {                                            
                perror("pthread_cancel failed");                                                                                                                
            }  
            int pj = pthread_join(datap_iter->thread, NULL);
            if (pj != 0) {
                perror("Attempt to pthread_join thread failed with\n");
                //return false;
            } 
        }


        pthread_mutex_destroy(&mutex);
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

void *add_timestamps(void *arg) {
    while (1) {
        // Avoid compilation warning
        if (arg == NULL)
        {
            // Get current hour
            time_t current_time;
            struct tm *time_info;
            char timestamp_str[64];
            
            time(&current_time);
            time_info = localtime(&current_time);

            strftime(timestamp_str, sizeof(timestamp_str), "timestamp:%a, %d %b %Y %H:%M:%S %z", time_info);
            
            // Abre el archivo y escribe el timestamp
            pthread_mutex_lock(&mutex);
            FILE *data_file = fopen(FILE_PATH, "a");
            if (data_file != NULL) {
                fprintf(data_file, "%s\n", timestamp_str);
                fclose(data_file);
            }
            pthread_mutex_unlock(&mutex);
            
            sleep(TIMESTAMP_INTERVAL);
        }
    }
}

void* threadfunc(void* thread_param) {
    char buffer[1024] = {0};
    FILE *data_file = NULL;
    struct sockaddr_in client_addr = {0};
    socklen_t client_addr_len = sizeof(client_addr);
    
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    slist_data_t *datap = (slist_data_t *)thread_param;
    


    if (getpeername(datap->client_socket, (struct sockaddr *)&client_addr, &client_addr_len) == 0) {
        syslog(LOG_INFO, "Accepted connection from %s", inet_ntoa(client_addr.sin_addr));
    }

    while(1) {
        ssize_t data_recv = recv(datap->client_socket, buffer, sizeof buffer, 0);

        if (data_recv < 0) {
            perror("Connection closed or error while receiving\n");
            break;
        } else if (data_recv == 0) {
            // Listen for incoming connections
            if (listen(sockfd1, BACKLOG) == -1) {
                perror("Error listening for connections");
                close(sockfd1);
                free(datap);
                pthread_exit(NULL);
                exit(-1);
            }

            struct sockaddr_in client_addr = {0};
            socklen_t client_addr_len = sizeof(client_addr);
            datap->client_socket = accept(sockfd1, (struct sockaddr *)&client_addr, &client_addr_len);
            if (datap->client_socket == -1) {
                perror("Error accepting connection");
                free(datap);
                pthread_exit(NULL);
                exit(-1);
            }
            continue;
        }



        //printf("Data received: %ld : %s \n", data_recv, buffer);
        
        //mutex lock
        int tl = pthread_mutex_lock(&mutex);
        if (tl != 0) {
            perror("Attempt to obtain mutex failed with\n");
        }


        if (writeFile(FILE_PATH, buffer, data_recv) == EXIT_FAILURE) {
            //exit(EXIT_FAILURE);
            perror("Error writing file");
        }
        int tu = pthread_mutex_unlock(&mutex);
        if (tu != 0) {
            perror("Attempt to unlock mutex failed with\n");
        }
        
        
        if(strstr(buffer, "\n") != NULL) {



            pthread_mutex_lock(&mutex);

            data_file = fopen(FILE_PATH, "r");
            if (data_file == NULL) {
                perror("Error opening data file");
                free(datap);
                pthread_exit(NULL);
                exit(-1);
            }
            char *line = NULL;
            size_t length = 0;
            ssize_t bytes_read = 0;
            while ((bytes_read = getline(&line, &length, data_file)) > 0) {
                if (send(datap->client_socket, line, bytes_read, 0) == -1) {
                    perror("Error sending data");
                    break;
                }
            }
            free(line);
            fclose(data_file);
            pthread_mutex_unlock(&mutex);
    
        }
        
      
        
    }
    
    

    
    close(datap->client_socket);
    free(datap);
    syslog(LOG_DEBUG, "Closed conection from %s", inet_ntoa(client_addr.sin_addr));
    pthread_exit(NULL);


}
static int handle_connection() {
    

    SLIST_INIT(&head);

    // Handle timestamp
    pthread_t timestamp_thread;
    if (pthread_create(&timestamp_thread, NULL, add_timestamps, NULL) != 0) {
        perror("Error creating timestamp thread");
        exit(EXIT_FAILURE);
    }


	while(1) {

        
        //write to list
        slist_data_t *datap = (slist_data_t *)malloc(sizeof(slist_data_t));
        //datap->completed = false;
        

    
		struct sockaddr_in client_addr = {0};
        socklen_t client_addr_len = sizeof(client_addr);
        if ((datap->client_socket = accept(sockfd1, (struct sockaddr *)&client_addr, &client_addr_len)) == -1) {
			perror("Error on accept() call \n");
            free(datap);
			exit(EXIT_FAILURE);
		}
        


        int pc = pthread_create(&datap->thread, NULL, threadfunc, datap);
        if (pc != 0) {
            perror("Thread can't be created\n");
            free(datap);
            close(datap->client_socket);
            exit(EXIT_FAILURE);
        } else {
            SLIST_INSERT_HEAD(&head, datap, entries);
        }
        
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

    int pmi;
    if ((pmi = pthread_mutex_init(&mutex, NULL)) != 0) {
        perror("Failed to initialize account mutex\n");
    }

    if (daemon_mode) {
        if (fork() == 0) {
            int ret = handle_connection();
            close(sockfd1);
            exit(ret);
        } else {
            exit(0);
        }
    } else {
        int ret = handle_connection();
        close(sockfd1);
        exit(ret);
    }
    
    
}