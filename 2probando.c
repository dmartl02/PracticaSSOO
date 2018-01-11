//LIBRERIAS
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <string.h>

//VARIABLES GLOBALES
pthread_mutex_t semaforoNuevoCompetidor;
int numeroAtletasCompeticion;
int contadorAtletas;
int finalizaCompeticion;
char id[10];
char msg[100];
FILE *logFile;
char *logFileName ="registroTiempos.log";

//FUNCIONES
void nuevoCompetidorATarima1(int sig);
void nuevoCompetidorATarima2(int sig);
void inicializaVariablesAtleta(int posicionPuntero, int id, int indispuesto, int tarima);
void *AccionesAtleta(void *arg);
void *AccionesTarima(void *arg);
void writeLogMessage(char *id,char *msg);
void finalizarCompeticion(int sig);
int generarAleatorio(int min, int max);

//STRUCTS
struct atleta {
	pthread_t atletaHilo;
	int id;
	int ha_Competido;
	int tarima_Asignada;
	int necesita_Beber;
};

struct atleta *punteroAtletas;

struct tarima {
	pthread_t tarimaHilo;
	int id;
};

struct tarima *punteroTarimas;

int main() {
	//Registramos las signal
	if(signal(SIGUSR1, nuevoCompetidorATarima1)==SIG_ERR){
		perror("Llamada a signal.");
		exit(-1);
	}
	if(signal(SIGUSR2, nuevoCompetidorATarima2)==SIG_ERR){
		perror("Llamada a signal.");
		exit(-1);
	}
	if(signal(SIGINT, finalizarCompeticion)==SIG_ERR){
		perror("Llamada a signal.");
		exit(-1);
	}

	//Inicializamos los recursos
	numeroAtletasCompeticion = 0;
	contadorAtletas = 0;
	finalizaCompeticion = 0;
	
	//Reservamos memoria para punteroAtletas y para punteroTarima
	punteroAtletas = (struct atleta*)malloc(10*sizeof(struct atleta)); 
	punteroTarimas = (struct tarima*)malloc(2*sizeof(struct tarima));

	//Creamos los 2 threads de tarimas
	int i;
	for (i = 0; i < 2; i++) {
		punteroTarimas[i].id = i+1;
		pthread_create(&punteroTarimas[i].tarimaHilo, NULL, AccionesTarima, (void *)&punteroTarimas[i].id);
	}

	//Creamos el archivo para los log en caso de que no exista, si existe lo sobreescribimos
	logFile = fopen(logFileName, "w");
	if(logFile == NULL){
		char *err = strerror(errno);
		printf("%s", err);
		fclose(logFile);
	}
	writeLogMessage("Comienzo","Competicion de powerlifting");

	//El programa espera hasta recibir una signal, en cuanto reciba SIGINT se finaliza el programa
	while(finalizaCompeticion != 1) {
		pause();
	}
	printf("El número de atletas en la competición es %d.\n", numeroAtletasCompeticion);
	return 0;
}

//Al recibir la signal SIGUSR1
void nuevoCompetidorATarima1(int a){
	if(signal(SIGUSR1,nuevoCompetidorATarima1)==SIG_ERR){
		exit(-1);
	}
	if (numeroAtletasCompeticion < 10) {
		numeroAtletasCompeticion++;
		//Inicializamos variables de atleta
		punteroAtletas[contadorAtletas].id = contadorAtletas+1;
		punteroAtletas[contadorAtletas].ha_Competido = 0;
		punteroAtletas[contadorAtletas].tarima_Asignada = 1;
		punteroAtletas[contadorAtletas].necesita_Beber = 0;
		//punteroAtletas[contadorAtletas].indispuesto = 0;
		//inicializaVariablesAtleta(contadorAtletas, contadorAtletas+1, 0, 1);
		pthread_create(&punteroAtletas[contadorAtletas].atletaHilo, NULL, AccionesAtleta, (void *)&punteroAtletas[contadorAtletas].id);
		contadorAtletas++;
	}
}

//Al recibir la signal SIGUSR2
void nuevoCompetidorATarima2(int a){
	if(signal(SIGUSR2,nuevoCompetidorATarima2)==SIG_ERR){
		exit(-1);
	}
	if (numeroAtletasCompeticion < 10) {
		numeroAtletasCompeticion++;
		//Inicializamos variables de atleta
		punteroAtletas[contadorAtletas].id = contadorAtletas+1;
		punteroAtletas[contadorAtletas].ha_Competido = 0;
		punteroAtletas[contadorAtletas].tarima_Asignada = 2;
		punteroAtletas[contadorAtletas].necesita_Beber = 0;
		//punteroAtletas[contadorAtletas].indispuesto = 0;
		//inicializaVariablesAtleta(contadorAtletas, contadorAtletas+1, 0, 2);
		pthread_create(&punteroAtletas[contadorAtletas].atletaHilo, NULL, AccionesAtleta, (void *)&punteroAtletas[contadorAtletas].id);
		contadorAtletas++;
	}
}

//Establecemos el valor inicial de los atributos de cada thread(atleta)
/*void inicializaVariablesAtleta(int posicionPuntero, int identificador, int indispuesto, int tarima){
	punteroAtletas[posicionPuntero].id = identificador;
	punteroAtletas[posicionPuntero].indispuesto = indispuesto;
	punteroAtletas[posicionPuntero].tarimaAsignada = tarima;
}*/

//Metodo para decir que hace el atleta al ser creado
void *AccionesAtleta(void *arg) {
	//Escribimos en el log que ha entrado un nuevo atleta
	sprintf(id, "atleta_%d", *(int *)arg);
	sprintf(msg, "Entra en la competicion, su tarima asignada es %d", punteroAtletas[(*(int *)arg)-1].tarima_Asignada);
	writeLogMessage(id, msg);
}

void *AccionesTarima(void *arg) {
	printf("acabo de crear la tarima %d.\n", *(int *)arg);
}

//Metodo para escribir en el log
void writeLogMessage(char *id, char *msg){
	//Calculamos la hora actual
	time_t now = time(0);
	struct tm *tlocal = localtime(&now);
	char stnow[19];
	strftime(stnow, 19, "%d/%m/%y %H:%M:%S", tlocal);
	
	//Escribimos en el log
	logFile = fopen(logFileName, "a");
	fprintf(logFile, "[%s] %s: %s\n", stnow, id, msg);
	fclose(logFile);	
}

//Cuando recibimos la signal SIGINT debemos finalizar el programa
void finalizarCompeticion(int sig) {
	printf("La competicion ha finalizado.\n");
	writeLogMessage("Final", "La competicion ha finalizado");
	finalizaCompeticion = 1;
}

//Metodo para generar numero aleatorios
int generarAleatorio(int min, int max){
	srand(time(NULL));
	int aleatorio = rand()%((max+1)-min)+min;
	return aleatorio;
}
