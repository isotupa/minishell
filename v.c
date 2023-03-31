/*-
 * main.c
 * Minishell C source
 * Shows how to use "obtain_order" input interface function.
 *
 * Copyright (c) 1993-2002-2019, Francisco Rosales <frosal@fi.upm.es>
 * Todos los derechos reservados.
 *
 * Publicado bajo Licencia de Proyecto Educativo Práctico
 * <http://laurel.datsi.fi.upm.es/~ssoo/LICENCIA/LPEP>
 *
 * Queda prohibida la difusión total o parcial por cualquier
 * medio del material entregado al alumno para la realización
 * de este proyecto o de cualquier material derivado de este,
 * incluyendo la solución particular que desarrolle el alumno.
 *
 * DO NOT MODIFY ANYTHING OVER THIS LINE
 * THIS FILE IS TO BE MODIFIED
 */

#include <signal.h>
#include <stddef.h>			/* NULL */
#include <stdio.h>			/* setbuf, printf */
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h> 
#include <sys/types.h> 
#include <sys/stat.h>
#include <sys/times.h>

extern int obtain_order();		/* See parser.y for description */

void default_signal();
int is_internal(char* command);
int execute_internal(char** argvv);
int get_argc(char** argvv);
void execute_piped_commands(char*** argvv, int bg);

char* internal[4] = {"cd", "umask", "time", "read"};
int mask;
struct time{
    clock_t tms_utime;  /* user time */
    clock_t tms_stime;  /* system time */
    clock_t tms_cutime; /* user time of children */
    clock_t tms_cstime; /* system time of children */
};

int main(void)
{
    char ***argvv = NULL;
    int argvc;
    char **argv = NULL;
    int argc;
    char *filev[3] = { NULL, NULL, NULL };
    int bg;
    int ret;


    setbuf(stdout, NULL);			/* Unbuffered */
    setbuf(stdin, NULL);

    while (1) {
        fprintf(stderr, "%s", "msh> ");	/* Prompt */
        ret = obtain_order(&argvv, filev, &bg);
        if (ret == 0) break;		/* EOF */
        if (ret == -1) continue;	/* Syntax error */
        argvc = ret - 1;		/* Line */
        if (argvc == 0) continue;	/* Empty line */
#if 1


        if(!bg) {
            struct sigaction act;
            act.sa_handler = SIG_DFL;
            act.sa_flags = 0;
            sigaction(SIGINT, &act, NULL);
            sigaction(SIGQUIT, &act, NULL);
        }


        int dupres[3];

        if(filev[0]) {
            int fdopen = open(filev[0], O_RDONLY);

            if(fdopen < 0) {
                perror("open");
                continue;
            } else {
                dupres[0] = dup(0);
                close(0);
                dup(fdopen);
                close(fdopen);
            }
        }

        if(filev[1]) {
            /* int fdopen = open(filev[1], O_WRONLY | O_CREAT | O_TRUNC, 0666); */
            int fdopen = creat(filev[1], 0666);

            if(fdopen < 0) {
                perror("open");
                continue;
            } else {
                dupres[1] = dup(1);
                close(1);
                dup(fdopen);
                close(fdopen);
            }
        }

        if(filev[2]) {
            /* int fdopen = open(filev[2], O_WRONLY | O_CREAT | O_TRUNC, 0666); */
            int fdopen = creat(filev[1], 0666);

            if(fdopen < 0) {
                perror("open");
                continue;
            } else {
                dupres[2] = dup(2);
                close(2);
                dup(fdopen);
                close(fdopen);
            }
        }

        execute_piped_commands(argvv, bg); 

        int i;
        for(i = 0; i < 3; ++i) {
            if(filev[i]) {
                close(i);
                dup(dupres[i]);
                close(dupres[i]);
            }
        }



        default_signal();


#endif
    }
    exit(0);
    return 0;
}


void default_signal() {
    struct sigaction act;
    act.sa_handler = SIG_IGN;
    act.sa_flags = 0;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGQUIT, &act, NULL);
}



int is_internal(char* command) {
    int i;

    for(i = 0; i < 3; ++i) {
        if(strcmp(command, internal[i]) == 0)  return i;
    }

    return -1;
}

int execute_internal(char** argv) {
    int index = is_internal(argv[0]);
    int argc = 0;
    while (argv[argc] != NULL) {
        argc++;
    }
    char cwd[1000];
    switch (index) {
        case 0: // cd
            if(argc == 1) {
                if(chdir(getenv("HOME")) < 0) perror("cd as");
            } else if(argc == 2) {
                char* dest = argv[1];
                if(chdir(argv[1]) < 0) perror("cd q");
            } else {
                perror("cd");
            }
            printf("%s\n", getcwd(cwd, 1000));
            break;
        case 1: // umask
            if(argc == 2) {
                int new;
                if((new = atoi(argv[1])) == 0 || strcmp(argv[1], "0") != 0) perror("umask");
                else if(new > 778) perror("umask");
                else mask = umask(strtol(argv[1], NULL, 8));
            } else if(argc < 1 || argc > 2) perror("umask");
            printf("%o\n", mask);
            break;
        case 2: // time

            break;
        case 3: // read
            break;
    }


    return 0;
}

void execute_piped_commands(char*** argvv, int bg) {
    int num_commands = 0;
    while (argvv[num_commands] != NULL) {
        num_commands++;
    }

    int lastpid;
    int pid;

    int pipes[num_commands - 1][2];

    for (int i = 0; i < num_commands; i++) {
        if (i < num_commands - 1) {
            if (pipe(pipes[i]) < 0) {
                perror("pipe");
                exit(EXIT_FAILURE);
            }
        }

        /* if(bg ||(is_internal(argvv[i][0]) != -1 && i != num_commands) || is_internal(argvv[i][0]) == -1) { */
        pid = fork();
        /* } */

        if (pid < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        }

        else if (pid == 0) {


            if (i > 0) {
                close(pipes[i - 1][1]);
                dup2(pipes[i - 1][0], 0);
            }

            if (i < num_commands -1) {
                close(pipes[i][0]);
                dup2(pipes[i][1], 1);
            }



            /* if(bg ||(is_internal(argvv[i][0]) != -1 && i != num_commands) || is_internal(argvv[i][0]) == -1) { */
            /* printf("%s\n", argvv[i][0]); */
            execvp(argvv[i][0], argvv[i]);
            //}
            perror(argvv[i][0]);
            exit(EXIT_FAILURE);
        }

        else if(pid > 0){
            /* if(bg ||(is_internal(argvv[i][0]) != -1 && i != num_commands) || is_internal(argvv[i][0]) == -1) { */
            /*     execute_internal(argvv[i]); */
            /* } */
            if(i == num_commands-1) lastpid = pid;
            for (int j = 0; j < i; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
        } else {
            perror("fork");
        }
    }

    int status;

    if(bg) {
        printf("[%d]\n", pid);
    } else {
        waitpid(lastpid, &status, 0);
    }
    /* while (wait(NULL) > 0); */
}
