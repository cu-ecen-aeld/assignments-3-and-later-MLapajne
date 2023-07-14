#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>
#include <libgen.h>
#include <syslog.h>
#include <stdlib.h>
#include <string.h>

extern int errno;




int main(int argc, char *argv[]) {
    
    openlog(NULL, 0, LOG_USER);
    syslog(LOG_DEBUG, "Writing %s to %s", argv[2], argv[1]);

    if ( argc != 3 ) { //0 je ime datoteke in tudi prvi element
        printf("ERROR invalid number of arguments \n");
        syslog(LOG_ERR, "Invalid number of arguments: %d", argc);
        exit(EXIT_FAILURE);
    }

    //problemi s pointerji
    char dir_path_copy[strlen(argv[1]) + 1];
    strcpy(dir_path_copy, argv[1]);

    const char *dir_name = dirname(dir_path_copy);
    DIR *dir = opendir(dir_name);
    if (dir == NULL) {
        int status = mkdir(dir_name, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH); 
        if (status != 0) {
            perror("Error creating directories");
            syslog(LOG_ERR, "Error creating directories: %s", dir_name);
            exit(EXIT_FAILURE);
        }
    } else {
        closedir(dir);
    }
    
    
    FILE *file = fopen(argv[1], "w");
    if (!file) {
        perror("Error creating file");
        syslog(LOG_ERR, "Error creating files: %s \n", argv[1]);
        exit(EXIT_FAILURE);
    } else {
        fputs(argv[2], file);
        fclose(file);
        printf("Success, file has been writen \n");

    }

    return 0;
    

}