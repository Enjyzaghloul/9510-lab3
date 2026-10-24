
/*
 * 
 *
 * Template file.
 * You will need to add more code here to execute the command table.
 *
 * NOTE: You are responsible for fixing any bugs this code may have!
 *
 */
#include <errno.h> //by me
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <glob.h>
#include <wordexp.h>
#include <vector>
#include <string>
#include <cctype>
#include <iostream>
#include <sstream>
#include "command.h"
#include "tokenizer.h"
//by me 
static void freeSimple(SimpleCommand *sc)
{
    if (!sc)
        return;
    for (int j = 0; j < sc->_numberOfArguments; j++)
        free(sc->_arguments[j]);
    free(sc->_arguments);
    delete sc;
}
//by me

//by me
static bool isWord(const Token &t)
{
    return t.type == TOKEN_COMMAND || t.type == TOKEN_ARGUMENT;
}
//by me

//by me 
static void addWord(SimpleCommand *sc, const Token &t, bool quoted)
{
    if (!quoted && t.value.find_first_of("*?") != std::string::npos)
    {
        glob_t g;
        if (glob(t.value.c_str(), GLOB_NOCHECK, NULL, &g) == 0)
        {
            for (size_t k = 0; k < g.gl_pathc; k++)
                sc->insertArgument(strdup(g.gl_pathv[k]));
            globfree(&g);
            return;
        }
    }
    sc->insertArgument(strdup(t.value.c_str()));
}//by me

//by me
static bool mergeQuotes(std::vector<Token> &tokens, std::vector<bool> &quoted)
{
    std::vector<Token> out;
    quoted.clear();

    for (size_t i = 0; i < tokens.size(); i++)
    {
        Token t = tokens[i];
        if (t.type != TOKEN_EOF && !t.value.empty() &&
            (t.value[0] == '"' || t.value[0] == '\''))
        {
            char q = t.value[0];
            std::string text = t.value.substr(1);
            bool closed = false;

            while (true)
            {
                if (!text.empty() && text.back() == q)
                {
                    text.pop_back();
                    closed = true;
                    break;
                }
                if (i + 1 >= tokens.size() || tokens[i + 1].type == TOKEN_EOF)
                    break;
                i++;
                text += " " + tokens[i].value;
            }

            if (!closed)
            {
                fprintf(stderr, "myshell: syntax error: unterminated quote\n");
                return false;
            }
            if (t.type != TOKEN_COMMAND) t.type = TOKEN_ARGUMENT;
            t.value = text;
            out.push_back(t);
            quoted.push_back(true);
        }
        else
        {
            out.push_back(t);
            quoted.push_back(false);
        }
    }
    tokens = out;
    return true;
}
//by me

//by me
void parse(std::vector<Token> &tokens)
{
    std::vector<bool> quoted;
    if (!mergeQuotes(tokens, quoted))
        return;

    Command currentCommand;
    SimpleCommand *cur = new SimpleCommand();
    bool afterPipe = false;

    auto fail = [&](const std::string &msg)
    {
        fprintf(stderr, "myshell: syntax error: %s\n", msg.c_str());
        currentCommand.clear();
        freeSimple(cur);
        cur = nullptr;
    };

    for (size_t i = 0; i < tokens.size(); i++)
    {
        Token &t = tokens[i];
        switch (t.type)
        {
        case TOKEN_COMMAND:
        case TOKEN_ARGUMENT:
            addWord(cur, t, quoted[i]);
            afterPipe = false;
            break;

        case TOKEN_PIPE:
            if (cur->_numberOfArguments == 0)
            {
                fail("missing command before '|'");
                return;
            }
            currentCommand.insertSimpleCommand(cur);
            cur = new SimpleCommand();
            afterPipe = true;
            break;

        case TOKEN_REDIRECT:
        case TOKEN_APPEND:
        case TOKEN_INPUT:
        case TOKEN_ERROR:
        case TOKEN_REDIRECT_AND_ERROR:
        {
            if (i + 1 >= tokens.size() || !isWord(tokens[i + 1]))
            {
                fail("missing file name after '" + t.value + "'");
                return;
            }
            const char *file = tokens[i + 1].value.c_str();

            if (t.type == TOKEN_INPUT)
            {
                if (currentCommand._inputFile) { fail("ambiguous input redirection"); return; }
                currentCommand._inputFile = strdup(file);
            }
            else if (t.type == TOKEN_ERROR)
            {
                if (currentCommand._errFile) { fail("ambiguous error redirection"); return; }
                currentCommand._errFile = strdup(file);
            }
            else
            {
                if (currentCommand._outFile) { fail("ambiguous output redirection"); return; }
                currentCommand._outFile = strdup(file);
                currentCommand._append = (t.type != TOKEN_REDIRECT);
                if (t.type == TOKEN_REDIRECT_AND_ERROR)
                {
                    if (currentCommand._errFile) { fail("ambiguous error redirection"); return; }
                    currentCommand._errFile = strdup(file);
                    currentCommand._out_error = 1;
                }
            }
            i++; // skip the file name token
            break;
        }

        case TOKEN_BACKGROUND:
            if (i + 1 >= tokens.size() || tokens[i + 1].type != TOKEN_EOF)
            {
                fail("'&' must be at the end of the command");
                return;
            }
            currentCommand._background = 1;
            break;

        case TOKEN_EOF:
            break;
        }
    }

    if (cur->_numberOfArguments > 0)
    {
        currentCommand.insertSimpleCommand(cur);
        cur = nullptr;
    }
    else if (afterPipe)
    {
        fail("missing command after '|'");
        return;
    }
    else if (currentCommand._numberOfSimpleCommands == 0)
    {
        if (currentCommand._outFile || currentCommand._inputFile ||
            currentCommand._errFile || currentCommand._background)
            fail("missing command");
        else
        {
            freeSimple(cur); // empty line
            currentCommand.clear();
        }
        return;
    }
    freeSimple(cur);

    currentCommand.execute();
}
//by me

SimpleCommand::SimpleCommand()
{
    _numberOfAvailableArguments = 5;
    _numberOfArguments = 0;
    _arguments = (char **)malloc(_numberOfAvailableArguments * sizeof(char *));
    _arguments[0] = NULL;//by me
}

void SimpleCommand::insertArgument(char *argument)
{
    if (_numberOfAvailableArguments == _numberOfArguments + 1)
    {
        _numberOfAvailableArguments *= 2;
        _arguments = (char **)realloc(_arguments,
                                      _numberOfAvailableArguments * sizeof(char *));
    }

    _arguments[_numberOfArguments] = argument;
    _arguments[_numberOfArguments + 1] = NULL;

    _numberOfArguments++;
}

Command::Command()
{
   /* _numberOfAvailableSimpleCommands = 1;
    _simpleCommands = (SimpleCommand **)
        malloc(_numberOfSimpleCommands * sizeof(SimpleCommand *));

    _numberOfSimpleCommands = 0;
    _outFile = 0;
    _inputFile = 0;
    _errFile = 0;
    _background = 0;*/

    //by me
    _numberOfAvailableSimpleCommands = 1;
    _numberOfSimpleCommands = 0;
    _simpleCommands = (SimpleCommand **)
        malloc(_numberOfAvailableSimpleCommands * sizeof(SimpleCommand *));

    _outFile = 0;
    _inputFile = 0;
    _errFile = 0;
    _out_error = 0;
    _append = 0;
    _background = 0;
    //by me
}

void Command::insertSimpleCommand(SimpleCommand *simpleCommand)
{
    if (_numberOfAvailableSimpleCommands == _numberOfSimpleCommands)
    {
        _numberOfAvailableSimpleCommands *= 2;
        _simpleCommands = (SimpleCommand **)realloc(_simpleCommands,
                                                    _numberOfAvailableSimpleCommands * sizeof(SimpleCommand *));
    }

    _simpleCommands[_numberOfSimpleCommands] = simpleCommand;
    _numberOfSimpleCommands++;
}

void Command::clear()
{
    /*for (int i = 0; i < _numberOfSimpleCommands; i++)
    {
        for (int j = 0; j < _simpleCommands[i]->_numberOfArguments; j++)
        {
            free(_simpleCommands[i]->_arguments[j]);
        }

        free(_simpleCommands[i]->_arguments);
        free(_simpleCommands[i]);
    }*/

    //by me
    for (int i = 0; i < _numberOfSimpleCommands; i++)
        freeSimple(_simpleCommands[i]);

    free(_simpleCommands);
    _numberOfAvailableSimpleCommands = 1;
    _simpleCommands = (SimpleCommand **)malloc(sizeof(SimpleCommand *));
    //by me

    if (_outFile)
    {
        free(_outFile);
    }

    if (_inputFile)
    {
        free(_inputFile);
    }

    if (_errFile)
    {
        free(_errFile);
    }

    _numberOfSimpleCommands = 0;
    _outFile = 0;
    _inputFile = 0;
    _errFile = 0;
    //by me
    _out_error = 0;
    _append = 0;
    //by me
    _background = 0;

}

void Command::print()
{
    printf("\n\n");
    printf("              COMMAND TABLE                \n");
    printf("\n");
    printf("  #   Simple Commands\n");
    printf("  --- ----------------------------------------------------------\n");

    for (int i = 0; i < _numberOfSimpleCommands; i++)
    {
        printf("  %-3d ", i);
        for (int j = 0; j < _simpleCommands[i]->_numberOfArguments; j++)
        {
            printf("\"%s\" \t", _simpleCommands[i]->_arguments[j]);
        }
        printf("\n");
    }
    

    printf("\n\n");
    printf("  Output       Input        Error        Err&Out       Background\n");
    printf("  ------------ ------------ ------------ ------------ ------------\n");
    printf("  %-12s %-12s %-12s %-12s %-12s\n", _outFile ? _outFile : "default",
           _inputFile ? _inputFile : "default", _errFile ? _errFile : "default", _out_error == 1 ? _errFile : "default"
           ,_background ? "YES" : "NO");
    printf("\n\n");
}

//by me
void Command::execute()
{
    if (_numberOfSimpleCommands == 0)
    {
        clear();
        return;
    }

    print();

    int n = _numberOfSimpleCommands;

    //  built-in commands: exit and cd
    if (n == 1)
    {
        char **a = _simpleCommands[0]->_arguments;

        if (strcmp(a[0], "exit") == 0)
        {
            printf("Good bye!!\n");
            fflush(stdout);
            clear();
            exit(0);
        }
        if (strcmp(a[0], "cd") == 0)
        {
            const char *dir = a[1] ? a[1] : getenv("HOME");
            if (!dir || chdir(dir) != 0)
                fprintf(stderr, "myshell: cd: %s: %s\n",
                        dir ? dir : "(HOME not set)", strerror(errno));
            clear();
            return;
        }
    }

    fflush(stdout);

    int prevRead = -1;   // pipe
    pid_t lastPid = -1;

    // loop for SimpleCommand 
    for (int i = 0; i < n; i++)
    {
        int fds[2] = {-1, -1};
        if (i < n - 1 && pipe(fds) < 0)         // create pipe for all but the last command
        {
            perror("myshell: pipe");
            break;
        }

        pid_t pid = fork();
        if (pid < 0)
        {
            perror("myshell: fork");
            if (fds[0] != -1) close(fds[0]);
            if (fds[1] != -1) close(fds[1]);
            break;
        }

        if (pid == 0)   //child process
        {
            signal(SIGINT, SIG_DFL);    //closed by cntrl+c
            signal(SIGCHLD, SIG_DFL);
            // input 
            if (prevRead != -1)
                dup2(prevRead, 0);// pipe
            else if (i == 0 && _inputFile)
            {
                int fd = open(_inputFile, O_RDONLY);
                if (fd < 0)
                {
                    fprintf(stderr, "myshell: %s: %s\n", _inputFile, strerror(errno));
                    _exit(1);
                }
                dup2(fd, 0);
                close(fd);
            }

            //  output 
            if (i < n - 1)
                dup2(fds[1], 1);                  //pipe
            else if (_outFile)
            {
                int flags = O_WRONLY | O_CREAT | (_append ? O_APPEND : O_TRUNC);
                int fd = open(_outFile, flags, 0644);
                if (fd < 0)
                {
                    fprintf(stderr, "myshell: %s: %s\n", _outFile, strerror(errno));
                    _exit(1);
                }
                dup2(fd, 1);
                close(fd);
            }

            // error 
            if (i == n - 1)
            {
                if (_out_error)
                    dup2(1, 2);                      //error to output
                else if (_errFile)
                {
                    int fd = open(_errFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd < 0)
                    {
                        fprintf(stderr, "myshell: %s: %s\n", _errFile, strerror(errno));
                        _exit(1);
                    }
                    dup2(fd, 2);
                    close(fd);
                }
            }

            // close unused file descriptors
            if (prevRead != -1) close(prevRead);
            if (fds[0] != -1) close(fds[0]);
            if (fds[1] != -1) close(fds[1]);

            char **a = _simpleCommands[i]->_arguments;
            execvp(a[0], a);

            // execvp failed
            if (errno == ENOENT)
                fprintf(stderr, "myshell: %s: command not found\n", a[0]);
            else
                fprintf(stderr, "myshell: %s: %s\n", a[0], strerror(errno));
            _exit(127);
        }

       //  parent process
        if (prevRead != -1) close(prevRead);
        if (fds[1] != -1) close(fds[1]);
        prevRead = fds[0];     
        lastPid = pid;
    }

    if (prevRead != -1)
        close(prevRead);

   // wait for the last child process if not in background
    if (!_background && lastPid > 0)
    {
        int status;
        while (waitpid(lastPid, &status, 0) < 0 && errno == EINTR)
            ;
    }

    clear();
}
//by me

void Command::prompt()
{
    std::string input;
    while (true)
    {
        printf("myshell>");
        fflush(stdout);
        if (!std::getline(std::cin, input))   // Ctrl-D
        {
            printf("\n");
            break;
        }
        std::vector<Token> tokens = tokenize(input);
        parse(tokens);
    }
}

Command Command::_currentCommand;
SimpleCommand *Command::_currentSimpleCommand;

//signal handler for SIGCHLD to reap child processes and log their termination

static int logFd = -1;   // log file descriptor

static void childHandler(int)
{
    int saved = errno;
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
        if (logFd >= 0)
        {
            char buf[100];
            int len = snprintf(buf, sizeof(buf),
                               "Child process %d terminated\n", (int)pid);
            write(logFd, buf, len);
        }
    }
    errno = saved;
}

int main()
{
    signal(SIGINT, SIG_IGN);    //ignore cntrl+c in parent

    logFd = open("shell.log", O_WRONLY | O_CREAT | O_APPEND, 0644);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = childHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);

    Command::_currentCommand.prompt();
    return 0;
}
