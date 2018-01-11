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

//CONSTANTES
#define ATLETAS 10
#define TARIMAS 2

//VARIABLES GLOBALES
pthread_mutex_t entradaCompeticion;
pthread_mutex_t escritura;
pthread_mutex_t entradaTarima;
int contadorAtletas;
int finalizaCompeticion;
int posicionesLibresEnCola[10];
int colaTarimas[10];
int podio[3];
char id[10];
char msg[100];
FILE *logFile;
char *logFileName ="registroTiempos.log";

//FUNCIONES
void nuevoCompetidor(int sig);
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
	int ha_Competido; //Variable para saber si ha terminado el levantamiento
	int tarima_Asignada;
	int necesita_Beber;
	int puntuacion;
};

struct atleta *punteroAtletas;

struct tarima {
	pthread_t tarimaHilo;
	int id;
	int ocupada;
};

struct tarima *punteroTarimas;

int main() {
	//Registramos las signal
	if(signal(SIGUSR1, nuevoCompetidor)==SIG_ERR){
		perror("Llamada a signal.");
		exit(-1);
	}
	if(signal(SIGUSR2, nuevoCompetidor)==SIG_ERR){
		perror("Llamada a signal.");
		exit(-1);
	}
	if(signal(SIGINT, finalizarCompeticion)==SIG_ERR){
		perror("Llamada a signal.");
		exit(-1);
	}

	//Inicializamos los recursos
	if (pthread_mutex_init(&entradaCompeticion, NULL) != 0) exit(-1);
	if (pthread_mutex_init(&escritura, NULL) != 0) exit(-1);
	if (pthread_mutex_init(&entradaTarima, NULL) != 0) exit(-1);
	contadorAtletas = 0;
	finalizaCompeticion = 0;
	int i;
	for (i = 0; i < ATLETAS; i++) {
		colaTarimas[i] = 50;
	}
	
	//Reservamos memoria para punteroAtletas y para punteroTarima
	punteroAtletas = (struct atleta*)malloc(ATLETAS*sizeof(struct atleta)); 
	punteroTarimas = (struct tarima*)malloc(TARIMAS*sizeof(struct tarima));

	//Creamos los 2 threads de tarimas
	/*for (i = 0; i < TARIMAS; i++) {
		punteroTarimas[i].id = i+1;
		punteroTarimas[i].ocupada = 0;
		pthread_create(&punteroTarimas[i].tarimaHilo, NULL, AccionesTarima, (void *)&punteroTarimas[i].id);
	}*/
	punteroTarimas[0].id = 1;
	punteroTarimas[0].ocupada = 0;
	punteroTarimas[1].id = 2;
	punteroTarimas[1].ocupada = 0;
	pthread_create(&punteroTarimas[0].tarimaHilo, NULL, AccionesTarima, (void *)&punteroTarimas[0].id);
	pthread_create(&punteroTarimas[1].tarimaHilo, NULL, AccionesTarima, (void *)&punteroTarimas[1].id);
	printf("tarima %d, ocupada %d\n", punteroTarimas[0].id, punteroTarimas[0].ocupada);
	printf("tarima %d, ocupada %d\n", punteroTarimas[1].id, punteroTarimas[1].ocupada);

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
	return 0;
}

//Al recibir la signal SIGUSR1 o SIGUSR2
void nuevoCompetidor(int sig){
	if(signal(SIGUSR1,nuevoCompetidor)==SIG_ERR){
		exit(-1);
	}
	if(signal(SIGUSR2,nuevoCompetidor)==SIG_ERR){
		exit(-1);
	}
	//pthread_mutex_lock(&entradaCompeticion);
	if (contadorAtletas < ATLETAS) {
		//Inicializamos variables de atleta
		punteroAtletas[contadorAtletas].id = contadorAtletas+1;
		punteroAtletas[contadorAtletas].ha_Competido = 0;
		if (sig == 10) {
			punteroAtletas[contadorAtletas].tarima_Asignada = 1;
		} 
		if (sig == 12) {
			punteroAtletas[contadorAtletas].tarima_Asignada = 2;
		}
		punteroAtletas[contadorAtletas].necesita_Beber = 0;
		punteroAtletas[contadorAtletas].puntuacion = 0;

		//Creamos el hilo de atleta
		pthread_create(&punteroAtletas[contadorAtletas].atletaHilo, NULL, AccionesAtleta, (void *)&punteroAtletas[contadorAtletas].id);
		contadorAtletas++;
	}
	//pthread_mutex_unlock(&entradaCompeticion);
}

//Metodo para decir que hace el atleta al ser creado
void *AccionesAtleta(void *arg) {
	printf("AccionesAtleta tarima %d, ocupada %d\n", punteroTarimas[0].id, punteroTarimas[0].ocupada);
	printf("AccionesAtleta tarima %d, ocupada %d\n", punteroTarimas[1].id, punteroTarimas[1].ocupada);
	int idAtleta = *(int *)arg;
	int posicion, i;
	//Guardamos los atletas en la cola
	for (i = 0; i < ATLETAS; i++) {
		if(punteroAtletas[i].id == idAtleta){
			posicion = i;
			break;
		}
	}
	//Escribimos en el log que ha entrado un nuevo atleta y cual es su tarima
	pthread_mutex_lock(&escritura);
	sprintf(id, "atleta_%d", idAtleta);
	sprintf(msg, "Entra en la competicion, su tarima asignada es %d", punteroAtletas[idAtleta-1].tarima_Asignada);
	writeLogMessage(id, msg);
	pthread_mutex_unlock(&escritura);

	printf("nuevo competidor, soy %d\n", idAtleta);

	//Comportamiento del atleta
	int num, j;
	do {  //Espera activa
			pthread_mutex_lock(&entradaTarima);
			for(j = 0; j < ATLETAS; j++){
				if(colaTarimas[j] == 50){
					colaTarimas[j] = posicion;
					break;
				}
			}
			pthread_mutex_unlock(&entradaTarima);
			num = generarAleatorio(1, 100);
			if (num <= 15) {
				printf("El thread ha finalizado, numero %d, mi id de hilo es %d.\n", num, idAtleta);
				//contadorAtletas--;
				//hay que quitar un atleta de la cola
				pthread_mutex_lock(&escritura);
				sprintf(id, "atleta_%d", idAtleta);
				sprintf(msg, "Se retira por deshidratacion y falta de electrolitos");
				writeLogMessage(id, msg);
				pthread_mutex_unlock(&escritura);
				pthread_exit(NULL);
			}
			sleep(3);
	} while(punteroAtletas[idAtleta].ha_Competido == 0);
	printf("AccionesAtleta FIN tarima %d, ocupada %d\n", punteroTarimas[0].id, punteroTarimas[0].ocupada);
	printf("AccionesAtleta FIN tarima %d, ocupada %d\n", punteroTarimas[1].id, punteroTarimas[1].ocupada);
}

void *AccionesTarima(void *arg) {
    int idTarima = *(int *)arg;
    int atletaActual, atletasAtendidos = 0;
    int porcentajeAleatorio, duermeAleatorio;
    int i;
    printf("AccionesTarima INICIO tarima %d, ocupada %d\n", punteroTarimas[0].id, punteroTarimas[0].ocupada);
    printf("AccionesTarima INICIO tarima %d, ocupada %d\n", punteroTarimas[1].id, punteroTarimas[1].ocupada);
    do {
        if (colaTarimas[0] != 50) {
            pthread_mutex_lock(&entradaTarima);
            atletaActual = colaTarimas[0];
            for(i = 1; i < ATLETAS; i++){
                colaTarimas[i-1] = colaTarimas[i];  //Hacemos espacio en la colaTarimas
            }
            colaTarimas[9] = 50;
            pthread_mutex_unlock(&entradaTarima);
            punteroAtletas[atletaActual].ha_Competido == 1;
            punteroTarimas[idTarima].ocupada = 1;
            //Codigo especifico para los atletas
            if (atletaActual != 50) {
                if (++atletasAtendidos == 4) {
                    sleep(10);
                    atletasAtendidos = 0;
                }
                printf("buenas gente soy %d\n", punteroAtletas[atletaActual].id);
                porcentajeAleatorio = generarAleatorio(1, 100);
                if (porcentajeAleatorio <= 80) {  //movimiento valido
                    punteroAtletas[atletaActual].puntuacion = generarAleatorio(60, 300); //si hay hueco en el podio

                    pthread_mutex_lock(&escritura);
                    sprintf(id, "atleta_%d", idAtleta);
                    sprintf(msg, "Se retira por deshidratacion y falta de electrolitos");
                    writeLogMessage(id, msg);
                    pthread_mutex_unlock(&escritura);

                    duermeAleatorio = generarAleatorio(2, 6);
                    sleep(duermeAleatorio);
                } else if (porcentajeAleatorio > 80 && porcentajeAleatorio <= 90 ) {  //movimiento nulo por indumentaria
                    punteroAtletas[atletaActual].puntuacion = 0;
                    duermeAleatorio = generarAleatorio(1, 4);
                    sleep(duermeAleatorio);
                } else {  //movimiento nulo por falta de fuerza
                    punteroAtletas[atletaActual].puntuacion = 0;
                    duermeAleatorio = generarAleatorio(6, 10);
                    sleep(duermeAleatorio);
                }
            }
            printf("entro dowhile e if, soy %d\n", idTarima);
        }
        printf("me voy a dormir, soy la tarima %d\n", idTarima);
        sleep(2);  //Espera activa del hilo tarima para no sobrecargar el procesador
    } while(punteroTarimas[idTarima].ocupada == 0);
    printf("accionestarima, mi id = %d, ocupado = %d\n", idTarima, punteroTarimas[idTarima].ocupada);
	/*int idTarima = *(int *)arg;
	int atletaActual, i;
	//Identificamos que tarima esta ejecutando el codigo
	pthread_mutex_lock(&entradaTarima);
	if (idTarima == 1) {  //Tarima1
		//Buscamos el atleta para competir
		printf("Voy a mirar si hay atletas para competir.\n");
		while(1) {
			printf("entro while, TARIMA %d\n", idTarima);
			for (i = 0; i < contadorAtletas; i++) {  //Comprobamos si los atletas han competido
				if(punteroAtletas[i].ha_Competido == 0) {
					atletaActual = i;
					punteroAtletas[i].ha_Competido = 1;
					printf("i = %d, ha_Competido = %d, tarima = %d\n", i+1, punteroAtletas[i].ha_Competido, idTarima);
					break;
				}
				break;
			}
			sleep(3);
		}

		//Cambiamos el flag
		punteroTarimas[idTarima].ocupada = 1;*/
		//Levantamiento del atleta
		//Guardamos en el log la hora del levantamiento
		//Dormimos a)80% mov valido (2 a 6 s)  b)10% nulo indumentaria (1 a 4 s) c)10% nulo falta de fuerza (6 a 10 s)
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
