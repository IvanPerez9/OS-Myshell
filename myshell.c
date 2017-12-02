#include <stdio.h>
#include "parser.h"
#include <sys/wait.h>
#include <stdlib.h> 
#include <unistd.h>
#include <errno.h>
#include <wait.h> 
#include <string.h> 
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>

/*
* Autores: Iván Pérez Huete - Carlos Olmo Sahuquillo
*
* 11/30/17
*/

// gcc -Wall -Wextra myshell.c libparsher_64.a -o myshell 
// 	fprintf(stderr,	"%s : No se encuentra el mandato\n" , line->commands[0].argv[0] ); 

int funcionRedireccion ( char * entrada , char * salida , char * error );
void mycd ();

#define CDCONTS "cd" // Constante para hacer cd 

tline * line;


int main(void) {

	char buf[1024];
	int i;
	pid_t pid;
	int ** tuberias;
	int rEntrada = dup(fileno(stdin));
	int rSalida = dup(fileno(stdout));
	int rError = dup(fileno(stderr));
	
	/* Señales, se tratan inmediatamente SIGINT == Ctrl+C y SIGQUIT == Ctrl + \ */
	// Ignoramos las señales
	signal(SIGINT , SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	
	printf("msh> ");
	
	while (fgets(buf, 1024, stdin)) {
		
		// Si hago background poner esto tambien, ignorar como en la minishell 
		signal(SIGINT,SIG_IGN);
        signal(SIGQUIT,SIG_IGN);
		
		// Acceso al atributo de un puntero de forma sencilla -> 

		line = tokenize(buf);		
		int status;


		if (line==NULL) {
			continue;
		}

		if (line->redirect_input != NULL) {
			funcionRedireccion ( line->redirect_input , NULL , NULL ); 
			//freopen(line->redirect_input , "r" , stdin );
		}
		if (line->redirect_output != NULL) {
			funcionRedireccion ( NULL , line->redirect_output , NULL ); 
		}
		if (line->redirect_error != NULL) {
			funcionRedireccion ( NULL , NULL , line->redirect_error ); 
		}
		if (line->background) {
			printf("comando a ejecutarse en background\n");
			return(1);
		} 

		// int execvp (const char *file , char *const argv[]); 

		// Para 1 solo comando, contampla CD 
		if(line->ncommands==1){
			
			//Señales, accion por defecto
				signal(SIGINT , SIG_DFL);
				signal(SIGQUIT, SIG_DFL);
			
			// Comprueba si es cd
			if (strcmp(line->commands[0].argv[0], CDCONTS) == 0){// Comprueba si es el mandato es cd, llama a subprograma
				mycd();
			}else{ // Ejecuta el comando, como ejercicio de ejecuta 
				pid = fork();
				if (pid < 0) {  // Error fork
					fprintf(stderr, "Falló el fork().\n%s\n", strerror(errno));
					exit(1);
				}
				else if (pid == 0) { // Hace hijo 	
					
					execvp(line->commands[0].filename, line->commands[0].argv ); // Si pasa del exec , es que hay error
					// devuelve - 1 y salta el error 
					fprintf(stderr, "Error al ejecutar el comando: %s\n", strerror(errno));
					exit(1);
					
				}else{ 	
					wait (&status);
					if (WIFEXITED(status) != 0)
						if (WEXITSTATUS(status) != 0)
							printf("El comando no se ejecutó correctamente\n");
				}
			}
			
		}else if(!(line->ncommands == 1)){ // Si son 2 comandos o más
				
			// Variable tuberia de memoria dinamica para los N-1 comandos que haya ( una tuberia cada 2 comandos )
			// Variable pid para los descriptores de fichero, irá cambiando con los forks 
				
			tuberias=(int**) malloc ((line->ncommands-1)*sizeof(int*));
			for(i=0;i<line->ncommands-1;i++){
				tuberias[i]=(int*)malloc(2*sizeof(int));
				pipe(tuberias[i]);
				printf("Tuberia ini %d\n" , i);
			}
			int pids;
			for(i=0;i<line->ncommands;i++){
				
				//Señales, accion por defecto
					signal(SIGINT , SIG_DFL);
					signal(SIGQUIT, SIG_DFL);
				
				// Hace un fork() para cada hijo. El numero va cambiando con los forks
				pids=fork();				
				if(pids<0){ //error
					fprintf(stderr, "Falló el fork().\n%s\n", strerror(errno));
					exit(1);
				} else if (pids==0){ // Sino error hace hijos
					
					if(i==0){ //Primer hjijo
						close(tuberias[i][0]);
						dup2(tuberias[i][1],1);
						execvp(line->commands[i].filename, line->commands[i].argv );

						printf("Error al ejecutar el comando: %s\n", strerror(errno));
						exit(1);
					}else if(i==(line->ncommands-1)){	// Utimo hijo
						close(tuberias[i-1][1]);
						dup2(tuberias[i-1][0],0);
						execvp(line->commands[i].argv[0], line->commands[i].argv);
						
						printf("Error al ejecutar el comando: %s\n", strerror(errno));
						exit(1);
					}else{	// Resto de hijos			
						close(tuberias[i][0]);	
						close(tuberias[i-1][1]);	
						dup2(tuberias[i-1][0],0);	
						dup2(tuberias[i][1],1);

						execvp(line->commands[i].argv[0], line->commands[i].argv);	

						printf("Error al ejecutar el comando: %s\n", strerror(errno));
						exit(1);
					}
				
				}else{	//Padre
					if(!(i==(line->ncommands-1))){ // Por esto la violacion del core
						close(tuberias[i][1]);
					}
				}
			}
			for(i=0;i<line->ncommands;i++){	
				waitpid(pids,&status,0);
				//fprintf(stderr,"waits: %s\n", strerror(errno));
			}
			//Liberamos la memoria reservada antes
			for(i=0;i<line->ncommands-1;i++){
				free(tuberias[i]);
			}
			free(tuberias);
			
		}
		
		// Restablecer las redirecciones
		
		if(line->redirect_input != NULL ){
			dup2(rEntrada , fileno(stdin));	
		}
		if(line->redirect_output != NULL ){
			dup2(rSalida , fileno(stdout));	
		}
		if(line->redirect_error != NULL ){
			dup2(rError , fileno(stderr));	
		}
		
		printf("msh> ");
	}
	return 0;
	
}


/*  entrada estandra (stdin 0) 
	salida estandar (stdout 1)
	error estandar (stderr 2)
*/

int funcionRedireccion ( char * entrada , char * salida , char * error ){
	
	int aux; // variable auxiliar para redirigir 
	
	// Mostrar Error si falla OJO 

	// Si es de entrada. Requisitos que ponerle para que te deje abrir y editar el documento
	if(entrada != NULL ) {
		aux = open (entrada , O_CREAT | O_RDONLY);
		if(aux == -1){
			fprintf( stderr , "%s : Error. %s\n" , entrada , strerror(errno)); // Mostrar error , -1 igual a NULL 
			return 1;
		} else { 
			dup2(aux,fileno(stdin)); // Redirreccion a 0, Entrada estandar 
		}	
	}
	
	// Si es de Salida
	if(salida != NULL ) {
		aux = creat (salida ,  S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );
		if(aux == -1){
			fprintf( stderr , "%s : Error. %s\n" , salida , strerror(errno)); // Mostrar error , -1 igual a NULL 
			return 1;
		} else { 
			dup2(aux,fileno(stdout)); // Redirreccion a 1, Salida estandar 
		}	
	}
	
	// Si es de error 
	if(error != NULL ) {
		aux = creat (error ,  S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
		if(aux == -1){
			fprintf( stderr , "%s : Error. %s\n" , error , strerror(errno)); // Mostrar error , -1 igual a NULL 
			return 1;
		} else { 
			dup2(aux,fileno(stderr)); // Redirreccion a 2, Error estandar 
		}	
	}
	
	return 0;
}


void mycd (){
	
	char *dir; // Variable de directorios 
	char buffer[512];
	
	if(line->commands[0].argc > 2) // No puedo hacer un cd a 2 directorios 
		{
		  fprintf(stderr,"Uso: %s directorio\n", line->commands[0].argv[0]);
		}
	if (line->commands[0].argc == 1) // Si vale 1 , no me pasan nada, osea nombre del programa.
	{
		dir = getenv("HOME");
		if(dir == NULL)
		{
			fprintf(stderr,"No existe la variable $HOME\n");	
		}
	}else {
		dir = line->commands[0].argv[1];
	}
	
	// Comprobar si es un directorio.
	if (chdir(dir) != 0) { // Sino es distinto de 0 lo hace normal el chdir 
		fprintf(stderr,"Error al cambiar de directorio: %s\n", strerror(errno)); // Los errores a llamada al sistema siempre se guardan en errno, y strerror explica el porque de errno.
	}
	printf( "El directorio actual es: %s\n", getcwd(buffer,-1));
}


















































