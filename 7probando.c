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
pthread_mutex_t entradaCompeticion;  //regula la entrada de atletas a la competicion
pthread_mutex_t escritura;  //regula la escritura en el log
pthread_mutex_t entradaTarima;  //regula la entrada de atletas a las tarimas
int contadorAtletas;
int finalizaCompeticion;  
int atletasTarima1;
int atletasTarima2;
int colaTarimas[10];
int mejoresPuntuaciones[3];
int mejoresIds[3];
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
	int ha_Competido;
	int tarima_Asignada;
	int necesita_Beber;
};

struct atleta *punteroAtletas;

int main() {
	char id[10];  //identificador para escribir en el log
	char msg[100];  //mensaje que se escribe en el log
	pthread_t tarima1, tarima2;
	int idTarima1, idTarima2;

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
	//if (pthread_mutex_init(&entradaCompeticion, NULL) != 0) exit(-1);
	if (pthread_mutex_init(&escritura, NULL) != 0) exit(-1);
	if (pthread_mutex_init(&entradaTarima, NULL) != 0) exit(-1);
	idTarima1 = 1;
	idTarima2 = 2;
	contadorAtletas = 0;
	finalizaCompeticion = 0;
	atletasTarima1 = 0;
	atletasTarima2 = 0;
	int i;
	for (i = 0; i < ATLETAS; i++) {  //damos a todas las posiciones de la cola 50 para luego filtar que atletas entran
		colaTarimas[i] = 50;
	}
	for (i = 0; i < 3; i++) {
		mejoresPuntuaciones[i] = 0;
	}
	for (i = 0; i < 3; i++) {
		mejoresIds[i] = 0;
	}
	
	//Reservamos memoria para punteroAtletas y para punteroTarima
	punteroAtletas = (struct atleta*)malloc(ATLETAS*sizeof(struct atleta)); 

	//Creamos los 2 threads de tarimas
	pthread_create(&tarima1, NULL, AccionesTarima, (void *)&idTarima1);
	pthread_create(&tarima2, NULL, AccionesTarima, (void *)&idTarima2);

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
	printf("el numero de atletas en la competicion es %d\n", contadorAtletas);
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
		//Creamos el hilo de atleta
		pthread_create(&punteroAtletas[contadorAtletas].atletaHilo, NULL, AccionesAtleta, (void *)&punteroAtletas[contadorAtletas].id);
		contadorAtletas++;
	}
	//pthread_mutex_unlock(&entradaCompeticion);
}

void *AccionesAtleta(void *arg) {
	char id[10];
	char msg[100];
	int idAtleta = *(int *)arg;
	int posicion, i;

	//Guardamos los atletas en la cola y escogemos
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
	do {  //Espera activa en la cual se mide el pulso
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
				pthread_mutex_lock(&escritura);
				sprintf(id, "atleta_%d", idAtleta);
				sprintf(msg, "Se retira por deshidratacion y falta de electrolitos");
				writeLogMessage(id, msg);
				pthread_mutex_unlock(&escritura);
				pthread_exit(0);
			}
			sleep(3);
	} while(punteroAtletas[idAtleta].ha_Competido == 0);
}

void *AccionesTarima(void *arg) {
	char id[10];
	char msg[100];
    int idTarima = *(int *)arg;
    int atletasAtendidos;
	int atletaActual;
    int eventoAleatorio, duermeAleatorio, puntuacionAleatorio, aguaAleatorio;
    int i, tarimaOcupada = 0;
	int auxiliarPuntuacion = 0, auxiliarId = 0;

    do {  //Espera activa en la que las tarimas buscan continuamente si hay atletas en la cola
        if (colaTarimas[0] != 50) {  //Forma de escoger los atletas 
            pthread_mutex_lock(&entradaTarima);
            atletaActual = colaTarimas[0];  //Identificamos el atleta
		int identificador=0;
		identificador= punteroAtletas[atletaActual].id;
//
///
////
/////
//////
///////
////////   El Atleta Actual Siempre es 0 y por tanto al sumarle 1, siempre pondra que el atleta es 1
/////////
//////////
///////////
////////////
/////////////

            for(i = 1; i < ATLETAS; i++){
                colaTarimas[i-1] = colaTarimas[i];  //Hacemos espacio en la colaTarimas
            }
            colaTarimas[9] = 50;
            pthread_mutex_unlock(&entradaTarima);

            punteroAtletas[atletaActual].ha_Competido == 1;  //Cambiamos flag
            tarimaOcupada = 1;
			//Guardamos el numero de atletas que suben a cada tarima
			if(idTarima == 1) {
				atletasTarima1++;
			} else {
				atletasTarima2++;
			}

            if (atletaActual != 50) {
                printf("Buenas gente soy %d\n", punteroAtletas[atletaActual].id);

				//Calculamos que sucede con el atleta
                eventoAleatorio = generarAleatorio(1, 100);
				printf("mi numero es %d\n", eventoAleatorio);
				//Inicio levantamiento
                pthread_mutex_lock(&escritura);
                sprintf(id, "atleta_%d", atletaActual+1);
                sprintf(msg, "Inicio del levantamiento");
                writeLogMessage(id, msg);
                pthread_mutex_unlock(&escritura);

	            if (eventoAleatorio <= 80) {  //Movimiento valido
					puntuacionAleatorio = generarAleatorio(60, 300);
				
                    duermeAleatorio = generarAleatorio(2, 6);
                    sleep(duermeAleatorio);

					//CALCULAMOS SI EL ATLETA TIENE QUE IR A BEBER AGUA				

					//Fin levantamiento
                    pthread_mutex_lock(&escritura);
		    punteroAtletas[atletaActual].ha_Competido == 2;
                    //sprintf(id, "atleta_%d", atletaActual+1);
		    sprintf(id, "atleta_%d", identificador);
                    sprintf(msg, "Fin del levantamiento, movimiento valido, puntuacion = %d, tiempo = %d", puntuacionAleatorio, duermeAleatorio);
                    writeLogMessage(id, msg);
                    pthread_mutex_unlock(&escritura);
					//Vemos si la puntuacion es digna de podio
					for (i = 2; i >= 0; i--) {
						if(puntuacionAleatorio >= mejoresPuntuaciones[i]) {
							if (i == 2) {
								auxiliarPuntuacion = mejoresPuntuaciones[2];
								mejoresPuntuaciones[2] = puntuacionAleatorio;
								mejoresPuntuaciones[0] = mejoresPuntuaciones[1];
								mejoresPuntuaciones[1] = auxiliarPuntuacion;
								auxiliarId = mejoresIds[2];
								mejoresIds[2] = atletaActual+1;
								mejoresIds[0] = mejoresIds[1];
								mejoresIds[1] = auxiliarId;
								break;
							} else if (i == 1) {
								auxiliarPuntuacion = mejoresPuntuaciones[1];
								mejoresPuntuaciones[1] = puntuacionAleatorio;
								mejoresPuntuaciones[0] = auxiliarPuntuacion;
								auxiliarId = mejoresIds[1];
								mejoresIds[1] = atletaActual+1;
								mejoresIds[0] = auxiliarId;
								break;
							} else {
								mejoresPuntuaciones[0] = puntuacionAleatorio;
								mejoresIds[0] = atletaActual+1;
								break;
							}
						}
					}
					atletasAtendidos++;

                } else if (eventoAleatorio > 80 && eventoAleatorio <= 90 ) {  //Nulo por indumentaria
					//punteroAtletas[atletaActual].puntuacion = 0;
                    duermeAleatorio = generarAleatorio(1, 4);
                    sleep(duermeAleatorio);

					//CALCULAMOS SI EL ATLETA TIENE QUE IR A BEBER AGUA				

					//Fin levantamiento
                    pthread_mutex_lock(&escritura);
                    sprintf(id, "atleta_%d", atletaActual+1);
                    sprintf(msg, "Fin del levantamiento, movimiento nulo por indumentaria, puntuacion = 0, tiempo = %d", duermeAleatorio);
                    writeLogMessage(id, msg);
                    pthread_mutex_unlock(&escritura);
					//Vemos si la puntuacion es digna de podio
					for (i = 2; i >= 0; i--) {
						if(puntuacionAleatorio >= mejoresPuntuaciones[i]) {
							if (i == 2) {
								auxiliarPuntuacion = mejoresPuntuaciones[2];
								mejoresPuntuaciones[2] = puntuacionAleatorio;
								mejoresPuntuaciones[0] = mejoresPuntuaciones[1];
								mejoresPuntuaciones[1] = auxiliarPuntuacion;
								auxiliarId = mejoresIds[2];
								mejoresIds[2] = atletaActual+1;
								mejoresIds[0] = mejoresIds[1];
								mejoresIds[1] = auxiliarId;
								break;
							} else if (i == 1) {
								auxiliarPuntuacion = mejoresPuntuaciones[1];
								mejoresPuntuaciones[1] = puntuacionAleatorio;
								mejoresPuntuaciones[0] = auxiliarPuntuacion;
								auxiliarId = mejoresIds[1];
								mejoresIds[1] = atletaActual+1;
								mejoresIds[0] = auxiliarId;
								break;
							} else {
								mejoresPuntuaciones[0] = puntuacionAleatorio;
								mejoresIds[0] = atletaActual+1;
								break;
							}
						}
					}
					atletasAtendidos++;

                } else {  //Nulo por falta de fuerza
                    //punteroAtletas[atletaActual].puntuacion = 0;
                    duermeAleatorio = generarAleatorio(6, 10);
                    sleep(duermeAleatorio);

					//CALCULAMOS SI EL ATLETA TIENE QUE IR A BEBER AGUA				

					//Fin levantamiento
                    pthread_mutex_lock(&escritura);
                    sprintf(id, "atleta_%d", atletaActual+1);
                    sprintf(msg, "Fin del levantamiento, movimiento nulo por falta de fuerza, puntuacion = 0, tiempo = %d", duermeAleatorio);
                    writeLogMessage(id, msg);
                    pthread_mutex_unlock(&escritura);
					//Vemos si la puntuacion es digna de podio
					for (i = 2; i >= 0; i--) {
						if(puntuacionAleatorio >= mejoresPuntuaciones[i]) {
							if (i == 2) {
								auxiliarPuntuacion = mejoresPuntuaciones[2];
								mejoresPuntuaciones[2] = puntuacionAleatorio;
								mejoresPuntuaciones[0] = mejoresPuntuaciones[1];
								mejoresPuntuaciones[1] = auxiliarPuntuacion;
								auxiliarId = mejoresIds[2];
								mejoresIds[2] = atletaActual+1;
								mejoresIds[0] = mejoresIds[1];
								mejoresIds[1] = auxiliarId;
								break;
							} else if (i == 1) {
								auxiliarPuntuacion = mejoresPuntuaciones[1];
								mejoresPuntuaciones[1] = puntuacionAleatorio;
								mejoresPuntuaciones[0] = auxiliarPuntuacion;
								auxiliarId = mejoresIds[1];
								mejoresIds[1] = atletaActual+1;
								mejoresIds[0] = auxiliarId;
								break;
							} else {
								mejoresPuntuaciones[0] = puntuacionAleatorio;
								mejoresIds[0] = atletaActual+1;
								break;
							}
						}
					}
					atletasAtendidos++;
                }

                if (atletasAtendidos == 4) {  //Miramos si le toca descansar a los jueces
                    pthread_mutex_lock(&escritura);
                    sprintf(id, "juez_%d", idTarima);
                    sprintf(msg, "Inicio del descanso");
                    writeLogMessage(id, msg);
                    pthread_mutex_unlock(&escritura);
					printf("Descanso, llevo %d atletas, soy %d.\n", atletasAtendidos, idTarima);
                    sleep(10);
					printf("Acabe de descansar soy %d.\n", idTarima);
                    pthread_mutex_lock(&escritura);
                    sprintf(id, "juez_%d", idTarima);
                    sprintf(msg, "Fin del descanso");
                    writeLogMessage(id, msg);
                    pthread_mutex_unlock(&escritura);
                    atletasAtendidos = 0;
                }
            }
            printf("entro dowhile e if, soy %d\n", idTarima);
        }
        printf("me voy a dormir, soy la tarima %d\n", idTarima);
	tarimaOcupada = 0;
        sleep(2);  //Espera activa del hilo tarima para no sobrecargar el procesador
    } while(tarimaOcupada == 0 &&  punteroAtletas[atletaActual].ha_Competido <= 1);
    printf("FIN WHILE accionestarima, mi id = %d\n", idTarima);
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
	char id[10];
	char msg[100];
	finalizaCompeticion = 1;
	sprintf(msg, "Han accedido %d atletas a tarima1 y %d a tarima2", atletasTarima1, atletasTarima2);
	writeLogMessage("informacion", msg);
	if (contadorAtletas == 0) {	
		writeLogMessage("informacion", "Ningun atleta ha participado en la competicion");
	} else if (contadorAtletas == 1) {
		sprintf(id, "atleta_%d", mejoresIds[2]);
		sprintf(msg, "Primer puesto con puntuacion = %d", mejoresPuntuaciones[2]);
		writeLogMessage(id, msg);		
	} else if (contadorAtletas == 2) {
		sprintf(id, "atleta_%d", mejoresIds[2]);
		sprintf(msg, "Primer puesto con puntuacion = %d", mejoresPuntuaciones[2]);
		writeLogMessage(id, msg);
		sprintf(id, "atleta_%d", mejoresIds[1]);
		sprintf(msg, "Segundo puesto con puntuacion = %d", mejoresPuntuaciones[1]);
		writeLogMessage(id, msg);
	} else {
		sprintf(id, "atleta_%d", mejoresIds[2]);
		sprintf(msg, "Primer puesto con puntuacion = %d", mejoresPuntuaciones[2]);
		writeLogMessage(id, msg);
		sprintf(id, "atleta_%d", mejoresIds[1]);
		sprintf(msg, "Segundo puesto con puntuacion = %d", mejoresPuntuaciones[1]);
		writeLogMessage(id, msg);
		sprintf(id, "atleta_%d", mejoresIds[0]);
		sprintf(msg, "Tercer puesto con puntuacion = %d", mejoresPuntuaciones[0]);
		writeLogMessage(id, msg);
	}
	writeLogMessage("Final", "La competicion ha finalizado");
}

//Metodo para generar numero aleatorios
int generarAleatorio(int min, int max){
	srand(time(NULL));
	int aleatorio = rand()%((max+1)-min)+min;
	return aleatorio;
}
