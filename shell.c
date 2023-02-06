#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <signal.h>
#define maxLen 1000
volatile unsigned long sz1 = sizeof(char *);
volatile unsigned long sz2 = sizeof(char **);

void shellCom(char *cmd, int *pos);

void space(char *cmd, int *pos)     //пропускаем пробелы
{
    while(cmd[*pos] == ' ')
    {
        (*pos)++;
    }
}

int specsym(char *cmd, int *pos)    //сравнения со спецсимволом
{
    if((cmd[*pos] != ' ') && (cmd[*pos] != '|') && (cmd[*pos] != '&') && (cmd[*pos] != '(') && 
    (cmd[*pos] != '>') && (cmd[*pos] != '<') && (cmd[*pos] != '\0') && (cmd[*pos] != '\n') && (cmd[*pos] != ';')) return 0;
    else return 1;
}

char *readn(char *cmd, int *pos)    //считываем имя от первого символа до спецсимвола
{
    char *c;
    int j = *pos, i = 0;
    while( (cmd[*pos] != ' ') && (cmd[*pos] != '|') && (cmd[*pos] != '&') && (cmd[*pos] != '(') && 
    (cmd[*pos] != '>') && (cmd[*pos] != '<') && (cmd[*pos] != '\0') &&(cmd[*pos] != '\n') &&(cmd[*pos] != ';'))
    {
        (*pos)++;
    }
    c = (char *)malloc((*pos) - j + 1);
    for(int i = 0; i < ((*pos) - j); i++)
    {
        c[i] = cmd[j+i];
    }
    c[(*pos) - j] = '\0';
    return c;
}

void perenap(char *cmd, int *pos)           //перенапр ввод/вывод
{
    for(int i = 0; i < 2; i++)      //мб перенапр и ввода и вывода
    {
        space(cmd,pos);
        if(cmd[*pos] == '>')
        {
            int append = 0;
            if(cmd[(*pos) + 1] == '>')
            {
                (*pos)++;
                append = 1;
            }
            int fd;
            char *filen;
            (*pos)++;
            space(cmd,pos);
            filen = readn(cmd,pos);
            if (access(filen, F_OK)) fd = open(filen,O_CREAT|O_RDWR,S_IWRITE|S_IREAD);
            else if(append) fd = open(filen,O_APPEND|O_RDWR);
            else fd = open(filen,O_RDWR|O_TRUNC);
            dup2(fd,1);
            free(filen);
        }else if(cmd[*pos] == '<')
        {
            int fd;
            char *filen;
            (*pos)++;
            space(cmd,pos);
            filen = readn(cmd,pos);
            fd = open(filen,O_RDONLY);
            dup2(fd,0);
            free(filen);
        }
    }
}

char ***lists(char *cmd, int *pos, int *j)      //собираем массив списков для конвейра
{
    char ***b = (char ***)malloc(sz2) , *c;
    int i = 0;
    (*j) = -1;
    do
    {
        (*j)++;
        b = (char ***)realloc(b,((*j)+1)*sz2);
        b[*j] = NULL;
        i = 0;
        do
        {
            space(cmd,pos);
            c = readn(cmd,pos);
            b[*j] = (char **)realloc(b[*j],(i+1)*sz1);
            b[*j][i] = c;
            i++;
            space(cmd,pos);
            if(specsym(cmd,pos))
            {
                b[*j] = (char **)realloc(b[*j],(i+1)*sz1);
                b[*j][i] = NULL;                 //null в конце списка для execv
                break;
            }
        }while(1);
        (*pos)++;с
    }while((cmd[(*pos)-1] == '|') && (cmd[*pos] != '|')); 
    (*pos)--;
    return b;  //b[][] - массив из списков для execv (execvp(lis[i][0] ,lis[i]);)
}

int conv(char *cmd, int *pos, char ***lis, int pr) //конвейер
{        
    pid_t last_pid, pid;
    for(int i = 0; i < pr; i++)
    {
        int fd[2];
        int oldfd0;
        pipe(fd);
        if((last_pid = fork()) < 0)
        {
            perror("fork");
            exit(1);
        }else if(!last_pid)
        {
            if(i != 0)
            {
                dup2(oldfd0, 0);
            }
            if(i < (pr-1))
            {
                dup2(fd[1],1);
            }
            close(fd[1]);
            close(fd[0]);
            //perror("grep");
            execvp(lis[i][0],lis[i]);
            perror("grep");
            exit(1);
        }
        oldfd0 = fd[0];
        close(fd[1]);
    }
    int status, last_status;
    while((pid = wait(&status)) != -1)
    {
        if(pid == last_pid) last_status = WEXITSTATUS(status);
    }    
    if(errno == ECHILD) errno = 0;      //допустимая ошибка
    return last_status;
}

int comm(char *cmd, int *pos)     //команда
{
    space(cmd,pos);
    if(cmd[*pos] != '(')
    {
        int fd1 = dup(1);
        int fd0 = dup(0);
        char ***lis;
        int pr, status;
        perenap(cmd,pos);
        lis = lists(cmd,pos,&pr);
        perenap(cmd,pos);
        pr++;
        status = conv(cmd,pos,lis,pr);
        for(int i = 0; i < pr; i++)         //освобождаем память 
        {   
            int j = 0;
            while(lis[i][j] != NULL)
            {
                free(lis[i][j]);
                j++;
            }
            free(lis[i][j]);
            free(lis[i]);
        }
        free(lis);
        dup2(fd1,1);
        dup2(fd0,0);
        return status;
    }
    else
    {
        int j = *pos, i = *pos, t = 0;
        char s[maxLen];
        for(int t = 0; t < maxLen; t++)
        {
            s[t] = ' ';
        }
        while(cmd[j] != ')')
        {
            j++;
            (*pos)++;
        }
        (*pos)++;
        space(cmd,pos);
        strncpy(s,&cmd[i+1],j-i-1);
        s[maxLen-2] = '\n';
        s[maxLen-1] = '\0';
        shellCom(s,&t);
        if(errno == ECHILD) errno = 0;
        return 0;
    }
}

void condComm(char *cmd, int *pos)  //команда с условным выполнением 
{
    int status;
    errno = 0;
    int j = (*pos);
    do
    {
        (*pos) = j;
        status = comm(cmd,pos);
        if((cmd[*pos] == '&') && (cmd[(*pos)+1] == '&'))
        {
            (*pos) += 2;
            j = (*pos);
            if((status == 0) && (errno == 0))
            {
                status = comm(cmd,pos);
            }else
            {
                while((cmd[*pos] != '&') && (cmd[*pos] != ';') && ((cmd[*pos] != '|') || (cmd[(*pos)+1] != '|')) && (cmd[*pos] != '\n'))
                {
                    (*pos)++;
                }
            }
        }else if((cmd[*pos] == '|') && (cmd[(*pos)+1] == '|'))
        {
            (*pos) += 2;
            j = (*pos);
            if((status != 0) || (errno != 0))
            {
                status = comm(cmd,pos);
            }else
            {
                while((cmd[*pos] != '&') && (cmd[*pos] != ';') && ((cmd[*pos] != '|') || (cmd[(*pos)+1] != '|')) && (cmd[*pos] != '\n'))
                {
                    (*pos)++;
                }
            }
        }
    }while(((cmd[*pos] == '&') && (cmd[(*pos)+1] =='&')) || ((cmd[*pos] == '|') && (cmd[(*pos)+1] == '|')));
}

void shellCom(char *cmd, int *pos)      //команда шелла(Шелл)
{
    do
    {
        int i = *pos;
        while((cmd[*pos] != ';') && ((cmd[*pos] != '&') || (cmd[(*pos)+1] == '&')) && (cmd[*pos] != '\n'))
        {
            if(cmd[*pos] == '(')
            {
                while(cmd[*pos] != ')')
                {
                    (*pos)++;
                }
            }
            if((cmd[*pos] == '&') && (cmd[(*pos)+1] == '&')) (*pos) += 2;
            else (*pos)++;
        }
        if(cmd[*pos] == '&') // фоновый процесс 
        {
            pid_t pid;
            if((pid = fork()) < 0)
            {
                perror("fork");
                exit(1);
            }
            else if(!pid)   // моделирование фонового процесса для condComm(cmd,&i);
            {
                signal(SIGINT,SIG_IGN);
                int fd = open("/dev/null",O_RDONLY);
                dup2(fd,0);
                condComm(cmd,&i);
                exit(0);
            }
        }  
        else condComm(cmd,&i);
        if(cmd[*pos] == '\n') break;
        (*pos)++;
        space(cmd,pos);
        if(cmd[*pos] == '\n') break;
    }while(1);
    while(wait(NULL) != -1){};
}

int main()
{
    while(1)
    {
        char cmd[maxLen];
        int *pos = malloc(sizeof(int));
        (*pos) = 0;
        fgets(cmd,maxLen,stdin);
        fflush(stdin);
        shellCom(cmd,pos);
        free(pos);
    }
}
