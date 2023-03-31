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

#include <limits.h>
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
#include <time.h>
#include <errno.h>

extern int obtain_order();		/* See parser.y for description */

void default_signal();
int is_internal(char* command);
int execute_internal(char** argvv);
int get_argc(char** argvv);
int redirect(char* filev[3]);
void restart_redirect(char* filev[3]);
void execute_piped_commands(char*** argvv, int argvc, int bg);
void change(char*** argvv);
void replace_substring(char *str, const char *sub, const char *replacement);


int dupres[3];

struct tms ticks;
clock_t globaluserticks = 0;
clock_t globalsystticks = 0;
clock_t globalrealticks;

int status = 0;

int main(void)
{
    globalrealticks = -times(&ticks);
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

        if(!redirect(filev)) continue;
        change(argvv);
        execute_piped_commands(argvv, argvc, bg); 
        restart_redirect(filev);
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
    char* internal[4] = {"cd", "umask", "time", "read"};
    for(i = 0; i < 4; ++i) {
        if(strcmp(command, internal[i]) == 0)  return i;
    }

    return -1;
}

int execute_internal(char** argv) {
    int index = is_internal(argv[0]);
    int argc = 0;
    status = 0;
    while (argv[argc] != NULL) {
        argc++;
    }
    switch (index) {
        case 0: // cd
            if(argc == 1) {
                if(chdir(getenv("HOME")) < 0) {
                    perror("cd");
                    status = 1;
                }
            } else if(argc == 2) {
                if(chdir(argv[1]) < 0) {
                    perror("cd");
                    status = 1;
                }
            } else {
                perror("cd");
                status = 1;
            }
            char cwd[1000];
            printf("%s\n", getcwd(cwd, 1000));
            break;
        case 1: // umask
            if(argc > 2) {
                perror("umask");
                status = 1;
            } else if(argc == 1) {
                printf("%o\n", umask(0));
            } else if(argv[1] != NULL) {
                if(atoi(argv[1]) == 0 && strcmp(argv[1], "0") != 0){
                    perror("umask");
                    status = 1;
                } else{
                    if(atoi(argv[1]) < 778){
                        long m = strtol(argv[1], NULL, 8);
                        printf("%o\n", umask(m));
                    } else{
                        perror("umask");
                        status = 1;
                    }

                }
            } else {
                perror("umask");
                status = 1;
            }
            break;
        case 2: // time
            if(argc > 2) {
                static clock_t startticks = 9;
                clock_t realticks, userticks, systticks;
                struct tms ticks;
                int pid;
                int tick = sysconf(_SC_CLK_TCK);
                realticks = -startticks;
                userticks = 0;
                systticks = 0;
                realticks = -times(&ticks);
                userticks = -ticks.tms_utime - ticks.tms_cutime;
                systticks = -ticks.tms_stime - ticks.tms_cstime;
                pid = fork();

                if(pid == 0) {
                    execvp(argv[1], &argv[1]);
                    perror(argv[0]);
                    status = 1;
                } else if(pid > 0) {
                    wait(NULL);

                    realticks += times(&ticks);
                    userticks += ticks.tms_utime + ticks.tms_cutime;
                    systticks += ticks.tms_stime + ticks.tms_cstime;

                    double realsecs = (double)realticks/tick;
                    double usersecs = (double)userticks/tick;
                    double syssecs = (double)systticks/tick;

                    double realmill = realsecs - (int)realsecs;
                    double usermill = usersecs - (int)usersecs;
                    double sysmill = syssecs - (int)syssecs;
                    printf("%d.%03du %d.%03ds %d.%03dr\n", (int)usersecs, (int)usermill, (int)syssecs, (int)sysmill, (int)realsecs, (int)realmill);

                } else {
                    perror("fork");
                    status = 1;
                }
            } else if(argc == 1) {
                globalrealticks = -times(&ticks);
                int tick = sysconf(_SC_CLK_TCK);
                globalrealticks = -times(&ticks);
                globaluserticks = -ticks.tms_utime - ticks.tms_cutime;
                globalsystticks = -ticks.tms_stime - ticks.tms_cstime;

                globalrealticks += times(&ticks);
                globaluserticks += ticks.tms_utime + ticks.tms_cutime;
                globalsystticks += ticks.tms_stime + ticks.tms_cstime;

                double realsecs = (double)globalrealticks/tick;
                double usersecs = (double)globaluserticks/tick;
                double syssecs = (double)globalsystticks/tick;

                double realmill = realsecs - (int)realsecs;
                double usermill = usersecs - (int)usersecs;
                double sysmill = syssecs - (int)syssecs;

                printf("%d.%03du %d.%03ds %d.%03dr\n", (int)usersecs, (int)usermill, (int)syssecs, (int)sysmill, (int)realsecs, (int)realmill);

            } else {
                perror("times");
                status = 1;
            }
            break;
        case 3: // read
            if(argc < 2) {
                perror("read");
                status = 1;
            } else {
                int i = 1;

                char input[2048];
                fgets(input, 2048, stdin);
                input[strlen(input)-1] = '\0';

                char* token;

                token = strtok(input, " ");
                /* token = strtok(input, "\t"); */

                while(token != NULL) {
                    if(argv[i] == NULL) {
                       break;
                    }
                    if(argv[i+1] == NULL) {
                        char* rest = malloc(strlen(input) + 1);
                        rest[0] = '\0';
                        while(token != NULL) {
                            strcat(rest, token);
                            token = strtok(NULL, " ");
                            if(token != NULL)
                                strcat(rest, " ");
                        }
                        token = malloc(strlen(rest)+1);
                        memcpy(token, rest, strlen(rest));
                    }
                    char *name = argv[i];
                    char *value = token;
                    int len = snprintf(NULL, 0, "%s=%s", name, value);
                    char *string = malloc(len + 1);
                    snprintf(string, len + 1, "%s=%s", name, value);
                    if(putenv(string) != 0) {
                        perror("putenv");
                        status = 1;
                        return 0;
                    }
                    if(argv[i+1] != NULL) token = strtok(NULL, " ");
                    i++;
                }

            }
            break;
        default:
            break;
    }

    return 0;
}

int redirect(char* filev[3]) {
    status = 0;

    if(filev[0]) {
        int fdopen = open(filev[0], O_RDONLY);

        if(fdopen < 0) {
            perror("open");
            status = 1;
            return 0;
        } else {
            dupres[0] = dup(0);
            close(0);
            dup(fdopen);
            close(fdopen);
        }
    }

    if(filev[1]) {
        int fdopen = creat(filev[1], 0666);

        if(fdopen < 0) {
            perror("open");
            status = 1;
            return 0;
        } else {
            dupres[1] = dup(1);
            close(1);
            dup(fdopen);
            close(fdopen);
        }
    }

    if(filev[2]) {
        int fdopen = creat(filev[1], 0666);

        if(fdopen < 0) {
            perror("open");
            status = 1;
            return 0;
        } else {
            dupres[2] = dup(2);
            close(2);
            dup(fdopen);
            close(fdopen);
        }
    }
    return 1;
}

void restart_redirect(char* filev[3]) {
    int i;
    for(i = 0; i < 3; ++i) {
        if(filev[i]) {
            close(i);
            dup(dupres[i]);
            close(dupres[i]);
        }
    }
}

void execute_piped_commands(char*** argvv, int argvc, int bg) {
    status = 0;
    int pipes[argvc - 1][2];
    for (int i = 0; i < argvc - 1; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("pipe");
            status = 1;
            exit(1);
        }
    }

    int pid, i, lastpid = -1, index = -1;

    for (i = 0; i < argvc; i++) {
        if(bg ||(is_internal(argvv[i][0]) != -1 && i != argvc-1) || is_internal(argvv[i][0]) == -1) {
            pid = fork();
        } else {
            index = i;
        }
        if (pid == -1) {
            perror("fork");
            status = 1;
            exit(1);
        } else if (pid == 0) {

            if (i > 0) {
                if (dup2(pipes[i - 1][0], 0) == -1) {
                    perror("dup2");
                    status = 1;
                    exit(1);
                }
            }
            if (i < argvc - 1) {
                if (dup2(pipes[i][1], 1) == -1) {
                    perror("dup2");
                    status = 1;
                    exit(1);
                }
            }

            for (int j = 0; j < argvc - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            if(is_internal(argvv[i][0]) != -1) {
                execute_internal(argvv[i]);
            } else {
                execvp(argvv[i][0], argvv[i]);
            }
            perror(argvv[i][0]);
            status = 1;
            exit(1);
        }
        if(i == argvc - 1) lastpid = pid;
    }

    for (int i = 0; i < argvc - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
    if((index != -1) ) {
        execute_internal(argvv[index]);
        index = -1;
    }

    if(bg) {
        printf("[%d]\n", pid);
    } else {
        int status;
        waitpid(lastpid, &status, 0);
    }

}

void replace_substring(char *str, const char *sub, const char *replacement)
{
    char *temp;
    char *p;
    int str_len = strlen(str);
    int sub_len = strlen(sub);
    int replacement_len = strlen(replacement);
    int temp_len = str_len + replacement_len - sub_len;

    temp = malloc(temp_len + 1);
    if (!temp) {
        perror("malloc");
        return;
    }

    if (!(p = strstr(str, sub)))
        return;

    strncpy(temp, str, p - str);
    temp[p - str] = '\0';

    sprintf(temp + (p - str), "%s%s", replacement, p + sub_len);
    strcpy(str, temp);

    free(temp);
}

char** find_dollar_strings(const char* str) {
    int i = 0;
    int num_strings = 0;
    char buffer[1024];
    char** strings = malloc(sizeof(char*));

    while (str[i] != '\0') {
        if (str[i] == '$') {
            int num_matched = sscanf(&str[i + 1], "%[_a-zA-Z0-9]", buffer);
            if (num_matched == 1) {
                strings[num_strings] = malloc(strlen(buffer) + 1);
                /* printf("a: %s\n", buffer); */
                strcpy(strings[num_strings], buffer);
                num_strings++;
                strings = realloc(strings, (num_strings + 1) * sizeof(char*));
            }
            i += strlen(buffer) + 1;
        } else {
            i++;
        }
    }

    if (num_strings == 0) {
        free(strings);
        return NULL;
    }

    strings[num_strings] = NULL;
    return strings;
}

void change(char*** argvv) {
    int i, j;
    char ** argv;

    for(i = 0; (argv = argvv[i]); i++) {
        for(j = 0; argv[j]; j++) {
            replace_substring(argv[j], "~", getenv("HOME"));
            replace_substring(argv[j], "$HOME", getenv("HOME"));
            /* char buffer1[16]; */
            /* sprintf(buffer1, "%d", status); */
            /* printf("%s\n", getenv("status")); */
            /* replace_substring(argv[j], "$status", getenv("status")); */
            char buffer[16];
            sprintf(buffer, "%d", getpid());
            replace_substring(argv[j], "$mypid", buffer);
            char** strings = find_dollar_strings(argv[j]);
            if (strings == NULL) {
                continue;
            } else {
                for (int i = 0; strings[i] != NULL; i++) {
                    char aux[strlen(strings[i]) + 2];
                    for (int j = strlen(strings[i]); j >= 0; j--) {
                        aux[j + 1] = strings[i][j];
                    }
                    aux[0] = '$';
                    if(getenv(strings[i]))
                        replace_substring(argv[j], aux, getenv(strings[i]));
                    free(strings[i]);
                }
                free(strings);
            }
        }
    }
}
