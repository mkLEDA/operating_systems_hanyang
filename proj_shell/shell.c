/**
  *  This file is shell.c, main code for 
  *  a simple implementation of Unix shell
  *  and simple utilisation of basic syscalls
  *
  *  @author Juliette Garreau
  *  @since 2016-03-22
  */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

/*
   * this function test (with a relative efficacity)
   * the input of a "quit" command, which will exit
   * the whole shell
   * @param char * : input of the string to test
   * @return int : 1 if it is the quit command, 0 otherwise
*/
int quit_command (char *token) {
    //Moving to the first character of the input
    while (*token == ' ') { 
        token++;
    }
    // Checking for the sequence "quit"
    if (*token == 'q') {
        token++;
        if (*token == 'u') {
            token++;
            if (*token == 'i') {
                token++;
                if (*token == 't'){
                    token++;
                    return 1;
                }
            }
        }
    }
    //If sequence not found, return 0
    return 0;
}

/*
   * main execution of the command line by parsing in tokens
   * create child processes executing the commands
   * @param char array : the input line to parse and execute
   * @return int : 1 if the shell must exit, 0 otherwise
*/
int execution (char chaine[100]) {
    char *token;

    // Parsing the given line until the first ";"
    token = strtok(chaine, ";");

    // Number of child proceses created
    int nbChild = 0;

    // Current token is "quit"
    int quit = 0;

    // Quit command was found in the tokens
    int overall_quit = 0;

    // While the line is not totally parsed, take new token
    while (token){
        //Testing if the token is the quit command
        quit =  quit_command(token);
        overall_quit = quit || overall_quit;

        int child_pid = 0;

        // If not quit, create child process
        if (!quit) {
            child_pid = fork();
            nbChild++;
        }

        // If error while creating subprocess, exit
        if (child_pid < 0 && !quit) {
            printf("Error with fork");
            exit(1);
        }
        // If child process, execute token with shell
        else if (child_pid == 0 && !quit) {
            char *instruction[4] = {
                "sh",
                "-c",
                strdup(token),
                NULL };
            execvp(instruction[0], instruction);
            
        }
        token = strtok(NULL, ";");
    }
    //Wait for all child processes to finish
    while(nbChild) {
        nbChild--;
        wait(NULL);
    }
    return overall_quit;  
}

/*
   * Main function
   * Taking file as input to execute shell commands
   * Or execute interactive mode if no input file given
   * @param int : number of input
   * @param char* array : file name or none
   * @return int : process executed correctly
*/
int main (int argc, char *argv[]) {
    //BASH MODE
    if (argc > 1) { // If input file, open the file
        FILE *fichier = NULL;
        fichier = fopen(argv[1], "r");
        if(!fichier){ // If the file does not exist, print error and exit
            printf("invalid file name, could not load.");
            exit(1);
        }
        char chaine[100] = "";
        int quit = 0;

        // While the process does not reach the end of the file, get line
        while (fgets(chaine,100, fichier) && !quit) {
            printf("%s", chaine);
            quit = execution(chaine); // Execute line
        }
    }

    // INTERACTIVE MODE
    else{ 
        printf("prompt>");
        char chaine[100] = ""; 
        fgets(chaine, 100, stdin);
        int quit = execution(chaine);
        // While given commands are not "quit", execute
        while(!quit){
            printf("prompt>");
            fgets(chaine, 100, stdin);
            quit = execution(chaine);
        }
    }
    return 0;    
}
