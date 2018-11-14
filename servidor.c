/*
** Fichero: servidor.c
** Autores:
** Francisco Blázquez Matías 
** David Pulido Macías 
*/

/*
 *                          S E R V I D O R
 *
 *        This is an example program that demonstrates the use of
 *        sockets TCP and UDP as an IPC mechanism.  
 *
 */
#include <sys/stat.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>


#define PUERTO 9093        //Puerto
#define ADDRNOTFOUND        0xffffffff        /* return address for unfound host */
#define BUFFERSIZE        1024        //Buffer de los paquetes enviados y recibidos
#define PATHSIZE 1000        //Tamaño de la ruta maxima
#define MAXHOST 128
#define TIMEOUT 6 //Timeout para la señal de alarma

extern int errno;

/*
 *                        M A I N
 *
 *        This routine starts the server.  It forks, leaving the child
 *        to do all the work, so it does not have to be run in the
 *        background.  It sets up the sockets.  It
 *        will loop forever, until killed by a signal.
 *
 */
 
void serverTCP(int s, struct sockaddr_in peeraddr_in);
void serverUDP(int s, char * buffer, struct sockaddr_in clientaddr_in);
void errout(char *);                /* declare error out routine */
void handler();

int FIN = 0;             /* Para el cierre ordenado */
void finalizar(){ FIN = 1; }

int main(argc, argv)
int argc;
char *argv[];
{

    int s_TCP, s_UDP;                /* connected socket descriptor */
    int ls_TCP;                                /* listen socket descriptor */
        int sCliUDP; //Socket para cada cliente udp
    
    int cc;                                    /* contains the number of bytes read */
     
    struct sigaction sa = {.sa_handler = SIG_IGN}; /* used to ignore SIGCHLD */
    
        struct sockaddr_in sCliUDP_in; // para el socket de cada cliente
    struct sockaddr_in myaddr_in;        /* for local socket address */
    struct sockaddr_in clientaddr_in;        /* for peer socket address */
        int addrlen;
        
    fd_set readmask;
    int numfds,s_mayor;
    
    char buffer[BUFFERSIZE];        /* buffer for packets to be read into */
    
    struct sigaction vec;

                /* Create the listen socket. */
        ls_TCP = socket (AF_INET, SOCK_STREAM, 0);
        if (ls_TCP == -1) {
                perror(argv[0]);
                fprintf(stderr, "%s: unable to create socket TCP\n", argv[0]);
                exit(1);
        }
        /* clear out address structures */
        memset ((char *)&myaddr_in, 0, sizeof(struct sockaddr_in));
           memset ((char *)&clientaddr_in, 0, sizeof(struct sockaddr_in));

    addrlen = sizeof(struct sockaddr_in);

                /* Set up address structure for the listen socket. */
        myaddr_in.sin_family = AF_INET;
                /* The server should listen on the wildcard address,
                 * rather than its own internet address.  This is
                 * generally good practice for servers, because on
                 * systems which are connected to more than one
                 * network at once will be able to have one server
                 * listening on all networks at once.  Even when the
                 * host is connected to only one network, this is good
                 * practice, because it makes the server program more
                 * portable.
                 */
        myaddr_in.sin_addr.s_addr = INADDR_ANY;
        myaddr_in.sin_port = htons(PUERTO);

        /* Bind the listen address to the socket. */
        if (bind(ls_TCP, (const struct sockaddr *) &myaddr_in, sizeof(struct sockaddr_in)) == -1) {
                perror(argv[0]);
                fprintf(stderr, "%s: unable to bind address TCP\n", argv[0]);
                exit(1);
        }
                /* Initiate the listen on the socket so remote users
                 * can connect.  The listen backlog is set to 5, which
                 * is the largest currently supported.
                 */
        if (listen(ls_TCP, 5) == -1) {
                perror(argv[0]);
                fprintf(stderr, "%s: unable to listen on socket\n", argv[0]);
                exit(1);
        }
        
        
        /* Create the socket UDP. */
        s_UDP = socket (AF_INET, SOCK_DGRAM, 0);
        if (s_UDP == -1) {
                perror(argv[0]);
                printf("%s: unable to create socket UDP\n", argv[0]);
                exit(1);
           }
        /* Bind the server's address to the socket. */
        if (bind(s_UDP, (struct sockaddr *) &myaddr_in, sizeof(struct sockaddr_in)) == -1) {
                perror(argv[0]);
                printf("%s: unable to bind address UDP\n", argv[0]);
                exit(1);
            }

                /* Now, all the initialization of the server is
                 * complete, and any user errors will have already
                 * been detected.  Now we can fork the daemon and
                 * return to the user.  We need to do a setpgrp
                 * so that the daemon will no longer be associated
                 * with the user's control terminal.  This is done
                 * before the fork, so that the child will not be
                 * a process group leader.  Otherwise, if the child
                 * were to open a terminal, it would become associated
                 * with that terminal as its control terminal.  It is
                 * always best for the parent to do the setpgrp.
                 */
        setpgrp(); //Desvincula el proceso del terminal abierto

        switch (fork()) {
        case -1:                /* Unable to fork, for some reason. */
                perror(argv[0]);
                fprintf(stderr, "%s: unable to fork daemon\n", argv[0]);
                exit(1);

        case 0:     /* The child process (daemon) comes here. */

                        /* Close stdin and stderr so that they will not
                         * be kept open.  Stdout is assumed to have been
                         * redirected to some logging file, or /dev/null.
                         * From now on, the daemon will not report any
                         * error messages.  This daemon will loop forever,
                         * waiting for connections and forking a child
                         * server to handle each one.
                         */
                fclose(stdin);
                fclose(stderr);

                        /* Set SIGCLD to SIG_IGN, in order to prevent
                         * the accumulation of zombies as each child
                         * terminates.  This means the daemon does not
                         * have to make wait calls to clean them up.
                         */
                if ( sigaction(SIGCHLD, &sa, NULL) == -1) {
            perror(" sigaction(SIGCHLD)");
            fprintf(stderr,"%s: unable to register the SIGCHLD signal\n", argv[0]);
            exit(1);
            }
            
                    /* Registrar SIGTERM para la finalizacion ordenada del programa servidor */
        vec.sa_handler = (void *) finalizar;
        vec.sa_flags = 0;
        if ( sigaction(SIGTERM, &vec, (struct sigaction *) 0) == -1) {
            perror(" sigaction(SIGTERM)");
            fprintf(stderr,"%s: unable to register the SIGTERM signal\n", argv[0]);
            exit(1);
            }
        
                while (!FIN) {
            /* Meter en el conjunto de sockets los sockets UDP y TCP */
            FD_ZERO(&readmask);
            FD_SET(ls_TCP, &readmask);
            FD_SET(s_UDP, &readmask);
            /* 
            Seleccionar el descriptor del socket que ha cambiado. Deja una marca en 
            el conjunto de sockets (readmask)
            */ 
                if (ls_TCP > s_UDP) s_mayor=ls_TCP;
                    else s_mayor=s_UDP;

            if ( (numfds = select(s_mayor+1, &readmask, (fd_set *)0, (fd_set *)0, NULL)) < 0) {
                if (errno == EINTR) {
                    FIN=1;
                            close (ls_TCP);
                            close (s_UDP);
                    perror("\nFinalizando el servidor. SeÃal recibida en elect\n "); 
                }
            }
            else { 

                                /* Comprobamos si el socket seleccionado es el socket TCP */
                                if (FD_ISSET(ls_TCP, &readmask)) {
                                        /* Note that addrlen is passed as a pointer
                                         * so that the accept call can return the
                                         * size of the returned address.
                                         */
                                        /* This call will block until a new
                                         * connection arrives.  Then, it will
                                         * return the address of the connecting
                                         * peer, and a new socket descriptor, s,
                                         * for that connection.
                                         */
                                        s_TCP = accept(ls_TCP, (struct sockaddr *) &clientaddr_in, &addrlen);
                                        if (s_TCP == -1) exit(1);
                                        switch (fork()) {
                                                case -1:        /* Can't fork, just exit. */
                                                        exit(1);
                                                case 0:                /* Child process comes here. */
                                                                        close(ls_TCP); /* Close the listen socket inherited from the daemon. */
                                                        serverTCP(s_TCP, clientaddr_in);
                                                        exit(0);
                                                default:        /* Daemon process comes here. */
                                                                /* The daemon needs to remember
                                                                 * to close the new accept socket
                                                                 * after forking the child.  This
                                                                 * prevents the daemon from running
                                                                 * out of file descriptor space.  It
                                                                 * also means that when the server
                                                                 * closes the socket, that it will
                                                                 * allow the socket to be destroyed
                                                                 * since it will be the last close.
                                                                 */
                                                        close(s_TCP);
                                        }
                                } /* De TCP*/
                                /* Comprobamos si el socket seleccionado es el socket UDP */
                                if (FD_ISSET(s_UDP, &readmask)) {
                                        /* This call will block until a new
                                        * request arrives.  Then, it will
                                        * return the address of the client,
                                        * and a buffer containing its request.
                                        * BUFFERSIZE - 1 bytes are read so that
                                        * room is left at the end of the buffer
                                        * for a null character.
                                        */
                                        cc = recvfrom(s_UDP, buffer, BUFFERSIZE, 0, (struct sockaddr *)&clientaddr_in, &addrlen);
                                        if ( cc == -1) {
                                                perror(argv[0]);
                                                printf("%s: recvfrom error\n", argv[0]);
                                                exit (1);
                                                }
                                        /* Make sure the message received is
                                        * null terminated.
                                        */
                                        buffer[cc]='\0';

                                        switch(fork()){
                                                case -1:
                                                        perror(argv[0]);
                                                        printf("Error en la creacion del socket del cliente");
                                                        exit(1);

                                                case 0:
                                                        /*Creamos un nuevo socket*/
                                                        sCliUDP = socket(AF_INET, SOCK_DGRAM, 0);
                                                        if(sCliUDP == -1){
                                                                perror(argv[0]);
                                                                printf("%s: unable to create socket UDP\n", argv[0]);
                                                                exit(1);
                                                        }
                                                        /*Rellenamos la estructura para bindear*/
                                                        sCliUDP_in.sin_family = AF_INET; //Asignacion de la familia de direccion
                                                        sCliUDP_in.sin_port = 0; //bind() elegirá un puerto aleatoriamente
                                                        sCliUDP_in.sin_addr.s_addr = INADDR_ANY; //pone la Ip del seridor automáticamente
                                                        /*Bindeamos el nuevo socket con la estructura que ha rellenado el recvfrom al recibir el mensaje del cliente*/
                                                        if (bind(sCliUDP, (struct sockaddr *) &sCliUDP_in, sizeof(struct sockaddr_in)) == -1) {
                                                                perror(argv[0]);
                                                                printf("%s: unable to bind address UDP\n", argv[0]);
                                                                exit(1);
                                                        }
                                                        serverUDP (sCliUDP, buffer, clientaddr_in);
                                                        exit(0);

                                                default:
                                                        close(sCliUDP);
                                        }
                                }
                        }
                }   /* Fin del bucle infinito de atención a clientes */
        /* Cerramos los sockets UDP y TCP */
        close(ls_TCP);
        close(s_UDP);
    
        printf("\nFin de programa servidor!\n");
        
        default:                /* Parent process comes here. */
                exit(0);
        }

}

/*
 *                                S E R V E R T C P
 *
 *        This is the actual server routine that the daemon forks to
 *        handle each individual connection.  Its purpose is to receive
 *        the request packets from the remote client, process them,
 *        and return the results to the client.  It will also write some
 *        logging information to stdout.
 *
 */
void serverTCP(int s, struct sockaddr_in clientaddr_in)
{
        int reqcnt = 0;                /* keeps count of number of requests */
        char buf[BUFFERSIZE];                /* This example uses BUFFERSIZE byte messages. */
        char hostname[MAXHOST];                /* remote host's name string */

        int len, status;
    struct hostent *hp;                /* pointer to host info for remote host */
    long timevar;                        /* contains time returned by time() */
    
    struct linger linger;                /* allow a lingering, graceful close; */
                                                /* used when setting SO_LINGER */
    
        char conexionRed[] = "HTTP/1.1";
        char caracteresRetorno[] = "\r\n";
        FILE *html;        //Puntero al archivo del archivo html a leer
        FILE *p;        //Puntero al archivo del registro
        DIR *dire;        //Puntero a directorio
        struct dirent *dt; //Estructura donde estará la información sobre el archivo que se esta "sacando" en cada momento
        char registro[]= "peticiones.log"; //Nombre del archivo del registro
        char pathToWorkspace[BUFFERSIZE]; //Ruta al directorio del codigo fuente
        char dirWeb[] = "/www"; //Nombre del directorio de ficheros html
        char pathToDirWeb[BUFFERSIZE]; //Ruta al directorio de ficheros html
        char pathToFileHTML[BUFFERSIZE];
        char envioCl[BUFFERSIZE]; //Datos que recibe el servidor del cliente
        char respuesta[BUFFERSIZE]; //Envio de respuesta al cliente
        char *str , *cachos; //Cadenas que guardarán las partes separadas del strtok
        char vCabecera[3][100]; //Vector que guarda la cabecera separada
        char vConnection[3][50]; //Vector que guarda la conexion separada
        char vCachos[4][200]; //Vector que guarda los trocos del mensaje del cliente separados por \r\n
        int q; //Variable que controla el vector de la cabecera
        char aux[BUFFERSIZE]; //Cadena auxiliar
        char aux2[50];
        int flaggie=0; //Flag para la comparacion en el directorio

        /*Conseguimos el directorio sobre el que estamos trabajando para utilizarlo mas adelante*/
        if (getcwd(pathToWorkspace, sizeof(pathToWorkspace)) == NULL){
                perror("getcwd() error");
        }

        /*Concatenamos la ruta del directorio web a la del directorio actual*/
        strcpy(pathToDirWeb, pathToWorkspace);
        strcat(pathToDirWeb, dirWeb);
        
        /* Look up the host information for the remote host
         * that we have connected with.  Its internet address
         * was returned by the accept call, in the main
         * daemon loop above.
         */
         
        status = getnameinfo((struct sockaddr *)&clientaddr_in,sizeof(clientaddr_in), hostname,MAXHOST,NULL,0,0);
        if(status){
                /* The information is unavailable for the remote
                 * host.  Just format its internet address to be
                 * printed out in the logging information.  The
                 * address will be shown in "internet dot format".
                 */
                /* inet_ntop para interoperatividad con IPv6 */
                if (inet_ntop(AF_INET, &(clientaddr_in.sin_addr), hostname, MAXHOST) == NULL){
                        perror(" inet_ntop \n");
                }
        }
    /* Log a startup message. */
    time (&timevar);
        /* The port number must be converted first to host byte
         * order before printing.  On most hosts, this is not
         * necessary, but the ntohs() call is included here so
         * that this program could easily be ported to a host
         * that does require it.
         */
        printf("Startup from %s port %u at %s", hostname, ntohs(clientaddr_in.sin_port), (char *) ctime(&timevar));

        /* Set the socket for a lingering, graceful close.
         * This will cause a final close of this socket to wait until all of the
         * data sent on it has been received by the remote host.
         */
        linger.l_onoff  =1;
        linger.l_linger =1;

        if (setsockopt(s, SOL_SOCKET, SO_LINGER, &linger, sizeof(linger)) == -1) {
                errout(hostname);
        }

        /*En caso que la cabecera del fichero log se tenga que poner solo una vez:*/
        /*Limpiamos la cadena*/
        strcpy(aux, "");
        strcpy(aux2, "");
        strcat(aux, hostname);
        strcat(aux, inet_ntoa(clientaddr_in.sin_addr)); //Metemos la IP
        strcat(aux, " ");
        strcat(aux, "TCP"); //Metemos protocolo de transporte
        strcat(aux, " ");
        snprintf(aux2, sizeof(aux2), "%d", clientaddr_in.sin_port);
        strcat(aux, aux2); //Metemos puerto efimero
        strcat(aux, " ");
        strcat(aux, ctime(&timevar)); //Metemos la fecha y hora

        /*Metemos en el archivo peticiones.log lo recibido*/
        if(NULL == (p = (fopen(registro, "a")))){

                fprintf(stderr, "No se ha podido abrir el fichero");

        }
        fputs(aux, p); //Ponemos en el fichero la cabecera
        fclose(p); //Cerramos el fichero

        /*Recibimos la estructura con la peticion del cliente*/
        while(len = recv(s, envioCl, BUFFERSIZE, 0)){
                /*Tratamos el posible error*/
                if (len == -1) errout(hostname);
                
                strcpy(aux, ""); //Vaciamos la cadena                

                /*Troceamos el mensaje por \r\n*/
                q=0;
                cachos = strtok(envioCl, caracteresRetorno);
                while(cachos != NULL){

                        strcpy(vCachos[q], cachos);
                        cachos = strtok(NULL, caracteresRetorno);
                        q++;

                }
                
                /*Separamos el envio de la cabecera*/
                q=0;
                str = strtok(vCachos[0], " ");
                while(str != NULL){
                
                        strcpy(vCabecera[q], str);
                        /*Seguimos con el siguiente token*/
                        str = strtok(NULL, " ");
                        q++;

                }

                /*Troceamos el contenido del connection*/
                q=0;
                str = strtok(vCachos[2], " ");
                while(str != NULL){
        
                        strcpy(vConnection[q], str);
                        /*Seguimos con el siguiente token*/
                        str = strtok(NULL, " ");
                        q++;

                }

                /*Metemos el HTTP para el fichero .log*/
                strcpy(aux2, "");
                strcat(aux2, conexionRed);
                strcat(aux2, " ");

                strcpy(respuesta, "");
                /*Metemos el HTTP al mensaje*/
                strcpy(respuesta, conexionRed);
                strcat(respuesta, " ");

                /*Conseguimos la orden que manda el cliente, si no es GET se devolverá el error*/
                if(strcmp(vCabecera[0], "GET") != 0){
                        /*501 NOT IMPLEMENTED*/
                        /*RESPUESTA: */
                        strcat(respuesta, "501 Not Implemented");
                        strcat(respuesta, caracteresRetorno);

                        /*Metemos tambien la cabecera para el fichero .log*/
                        strcat(aux2, "501 Not Implemented");
                        strcat(aux2, caracteresRetorno);

                        /*SERVER: */
                        strcat(respuesta, "Server: ");
                        strcat(respuesta, hostname);
                        strcat(respuesta, caracteresRetorno);
                        
                        /*CONNECTION: */
                        if(strcmp(vConnection[1], "keep-alive") == 0){
                                /*NO Se cierra la conexion*/
                                strcat(respuesta, "Connection: ");
                                strcat(respuesta, "keep-alive");
                                strcat(respuesta, caracteresRetorno);

                                /*FINAL: */
                                strcat(respuesta, caracteresRetorno);

                                /*HTML: */
                                strcat(respuesta, "<html><body><h1>501 Not Implemented</h1></body></html>\n");

                                sleep(1); //Tiempo de trabajo del servidor

                                /*Enviamos la respuesta al cliente*/
                                if (send(s, respuesta, BUFFERSIZE, 0) != BUFFERSIZE) errout(hostname);

                                /* Incrementamos el contador de peticiones */
                                reqcnt++;

                                /*Metemos en el .log*/
                                if(NULL == (p = (fopen(registro, "a")))){

                                        fprintf(stderr, "No se ha podido abrir el fichero");

                                }
                                fputs(aux2, p);
                                fclose(p);

                        }
                        else{
                                /*Se cierra la conexion*/
                                strcat(respuesta, "Connection: ");
                                strcat(respuesta, "close");
                                strcat(respuesta, caracteresRetorno);

                                /*FINAL: */
                                strcat(respuesta, caracteresRetorno);

                                /*HTML: */
                                strcat(respuesta, "<html><body><h1>501 Not Implemented</h1></body></html>\n");

                                sleep(1); //Tiempo de trabajo del servidor

                                /*Enviamos la respuesta al cliente*/
                                if (send(s, respuesta, BUFFERSIZE, 0) != BUFFERSIZE) errout(hostname);

                                /* Incrementamos el contador de peticiones */
                                reqcnt++;
                                /*Cerramos la conexion*/
                                close(s);

                                /*Metemos en el .log*/
                                if(NULL == (p = (fopen(registro, "a")))){

                                        fprintf(stderr, "No se ha podido abrir el fichero");

                                }
                                fputs(aux2, p);
                                fclose(p);
                        }

                }
                else{ 
                        strcpy(pathToFileHTML, pathToDirWeb);
                        strcat(pathToFileHTML, vCabecera[1]);
                        if(NULL == (html = (fopen(pathToFileHTML, "r")))){
                                 flaggie = 0;
                        }
                        else{
                                 flaggie = 1;
                        }

                        /*Si se ha encontrado el fichero (flaggie = true) se responde y se saca lo de dentro*/
                        if(flaggie){
                                /*200 OK*/
                                /*RESPUESTA: */
                                strcat(respuesta, "200 OK");
                                strcat(respuesta, caracteresRetorno);

                                /*Metemos tambien la cabecera para el fichero .log*/
                                strcat(aux2, "200 OK");
                                strcat(aux2, caracteresRetorno);


                                /*SERVER: */
                                strcat(respuesta, "Server: ");
                                strcat(respuesta, hostname);
                                strcat(respuesta, caracteresRetorno);

                                /*CONNECTION: */
                                if(strcmp(vConnection[1], "keep-alive") == 0){

                                        /*NO Se cierra la conexion*/
                                        strcat(respuesta, "Connection: ");
                                        strcat(respuesta, "keep-alive");
                                        strcat(respuesta, caracteresRetorno);

                                        /*FINAL: */
                                        strcat(respuesta, caracteresRetorno);

                                        /*HTML: */
                                        /*Vamos leyendo el fichero y almacenando en el apartado html de la estructura para mandarlo*/
                                        while(fgets(buf, BUFFERSIZE, html) != NULL){

                                                strcat(respuesta, buf);

                                        }
                                        fclose(html);

                                        sleep(1); //Tiempo de trabajo del servidor

                                        /*Enviamos la respuesta al cliente*/
                                        if (send(s, respuesta, BUFFERSIZE, 0) != BUFFERSIZE) errout(hostname);

                                        /* Incrementamos el contador de peticiones */
                                        reqcnt++;

                                        /*Metemos en el .log*/
                                        if(NULL == (p = (fopen(registro, "a")))){

                                                fprintf(stderr, "No se ha podido abrir el fichero");

                                        }
                                        fputs(aux2, p);
                                        fclose(p);

                                }
                                else{
                                        /*Se cierra la conexion*/
                                        strcat(respuesta, "Connection: ");
                                        strcat(respuesta, "close");
                                        strcat(respuesta, caracteresRetorno);

                                        /*FINAL: */
                                        strcat(respuesta, caracteresRetorno);

                                        /*HTML: */
                                        /*Vamos leyendo el fichero y almacenando en el apartado html de la estructura para mandarlo*/
                                        while(fgets(buf, BUFFERSIZE, html) != NULL){

                                                strcat(respuesta, buf);

                                        }
                                        fclose(html);

                                        sleep(1); //Tiempo de trabajo del servidor

                                        /*Enviamos la respuesta al cliente*/
                                        if (send(s, respuesta, BUFFERSIZE, 0) != BUFFERSIZE) errout(hostname);

                                        /* Incrementamos el contador de peticiones */
                                        reqcnt++;
                                        /*Cerramos la conexion*/
                                        close(s);

                                        /*Metemos en el .log*/
                                        if(NULL == (p = (fopen(registro, "a")))){

                                                fprintf(stderr, "No se ha podido abrir el fichero");

                                        }
                                        fputs(aux2, p);
                                        fclose(p);
                                }
                        }
                        else{
                                /*404 Not found*/
                                /*RESPUESTA: */
                                strcat(respuesta, "404 Not found");
                                strcat(respuesta, caracteresRetorno);

                                /*Metemos tambien la cabecera para el fichero .log*/
                                strcat(aux2, "404 Not found");
                                strcat(aux2, caracteresRetorno);

                                /*SERVER: */
                                strcat(respuesta, "Server: ");
                                strcat(respuesta, hostname);
                                strcat(respuesta, caracteresRetorno);

                                /*CONNECTION: */
                                if(strcmp(vConnection[1], "keep-alive") == 0){
                                        /*NO Se cierra la conexion*/
                                        strcat(respuesta, "Connection: ");
                                        strcat(respuesta, "keep-alive");
                                        strcat(respuesta, caracteresRetorno);

                                        /*FINAL: */
                                        strcat(respuesta, caracteresRetorno);

                                        /*HTML: */
                                        strcat(respuesta, "<html><body><h1>404 Not Found</h1></body></html>\n");

                                        sleep(1); //Tiempo de trabajo del servidor

                                        /*Enviamos la respuesta al cliente*/
                                        if (send(s, respuesta, BUFFERSIZE, 0) != BUFFERSIZE) errout(hostname);

                                        /* Incrementamos el contador de peticiones */
                                        reqcnt++;

                                        /*Metemos en el .log*/
                                        if(NULL == (p = (fopen(registro, "a")))){

                                                fprintf(stderr, "No se ha podido abrir el fichero");

                                        }
                                        fputs(aux2, p);
                                        fclose(p);

                                }
                                else{
                                        /*Se cierra la conexion*/
                                        strcat(respuesta, "Connection: ");
                                        strcat(respuesta, "close");
                                        strcat(respuesta, caracteresRetorno);

                                        /*FINAL: */
                                        strcat(respuesta, caracteresRetorno);

                                        /*HTML: */
                                        strcat(respuesta, "<html><body><h1>404 Not Found</h1></body></html>\n");

                                        sleep(1); //Tiempo de trabajo del servidor

                                        /*Enviamos la respuesta al cliente*/
                                        if (send(s, respuesta, BUFFERSIZE, 0) != BUFFERSIZE) errout(hostname);

                                        /* Incrementamos el contador de peticiones */
                                        reqcnt++;
                                        /*Cerramos la conexion*/
                                        close(s);

                                        /*Metemos en el .log*/
                                        if(NULL == (p = (fopen(registro, "a")))){

                                                fprintf(stderr, "No se ha podido abrir el fichero");

                                        }
                                        fputs(aux2, p);
                                        fclose(p);
                                }
                        }
                }
        }
        
        close(s);

        time (&timevar);
        /* The port number must be converted first to host byte
         * order before printing.  On most hosts, this is not
         * necessary, but the ntohs() call is included here so
         * that this program could easily be ported to a host
         * that does require it.
         */
        printf("Completed %s port %u, %d requests, at %s\n", hostname, ntohs(clientaddr_in.sin_port), reqcnt, (char *) ctime(&timevar));
}

/*
 *        This routine aborts the child process attending the client.
 */
void errout(char *hostname)
{
        printf("Connection with %s aborted on error\n", hostname);
        exit(1);     
}


/*
 *                                S E R V E R U D P
 *
 *        This is the actual server routine that the daemon forks to
 *        handle each individual connection.  Its purpose is to receive
 *        the request packets from the remote client, process them,
 *        and return the results to the client.  It will also write some
 *        logging information to stdout.
 *
 */
void serverUDP(int s, char * buffer, struct sockaddr_in clientaddr_in){
    struct in_addr reqaddr;        /* for requested host's address */
    struct hostent *hp;                /* pointer to host info for requested host */
    int nc, errcode, len, len1;

    struct addrinfo hints, *res;

        int addrlen;

        struct sigaction alarma;
        char hostname[MAXHOST];                /* remote host's name string */
        int reqcnt=0; //Contador de peticiones
        char aux[BUFFERSIZE];
        char aux2[500];
        char buf[BUFFERSIZE];                /* This example uses BUFFERSIZE byte messages. */
        struct linger linger;
        long timevar;                        /* contains time returned by time() */
        char conexionRed[] = "HTTP/1.1";
        char caracteresRetorno[] = "\r\n";
        FILE *html;        //Puntero al archivo del archivo html a leer
        FILE *p;        //Puntero al archivo del registro
        DIR *dire;        //Puntero a directorio
        struct dirent *dt; //Estructura donde estará la información sobre el archivo que se esta "sacando" en cada momento
        char registro[]= "peticiones.log"; //Nombre del archivo del registro
        char pathToWorkspace[BUFFERSIZE]; //Ruta al directorio del codigo fuente
        char dirWeb[] = "/www"; //Nombre del directorio de ficheros html
        char pathToDirWeb[BUFFERSIZE]; //Ruta al directorio de ficheros html
        char pathToFileHTML[BUFFERSIZE];
        char respuesta[BUFFERSIZE]; //Envio de respuesta al cliente
        char envioCl[BUFFERSIZE]; //Envio del cliente
        char *str , *cachos; //Cadenas que guardarán las partes separadas del strtok
        char vCabecera[3][100]; //Vector que guarda la cabecera separada
        char vConnection[3][50]; //Vector que guarda la conexion separada
        char vCachos[4][200]; //Vector que guarda los trocos del mensaje del cliente separados por \r\n
        int q; //Variable que controla el vector de la cabecera
        int flaggie=0; //Flag para la comparacion en el directorio
        int socketON=1; //Flag que contola el estado del socket(abierto:true/cerrado:false)

        strcpy(envioCl, buffer);

        /*Conseguimos el directorio sobre el que estamos trabajando para utilizarlo mas adelante*/
        if (getcwd(pathToWorkspace, sizeof(pathToWorkspace)) == NULL){
                perror("getcwd() error");
        }

        /*Concatenamos la ruta del directorio web a la del directorio actual*/
        strcpy(pathToDirWeb, pathToWorkspace);
        strcat(pathToDirWeb, dirWeb);        

           addrlen = sizeof(struct sockaddr_in);

    memset (&hints, 0, sizeof (hints));

    hints.ai_family = AF_INET;

        time (&timevar);

        printf("Startup port %u at %s", ntohs(clientaddr_in.sin_port), (char *) ctime(&timevar));

        linger.l_onoff  =1;
        linger.l_linger =1;

        if (setsockopt(s, SOL_SOCKET, SO_LINGER, &linger, sizeof(linger)) == -1) {
                errout(hostname);
        }

        /* Registrar SIGALRM para no quedar bloqueados en los recvfrom */
    alarma.sa_handler = (void *) handler;
    alarma.sa_flags = 0;
    if ( sigaction(SIGALRM, &alarma, (struct sigaction *) 0) == -1) {
        perror(" sigaction(SIGALRM)");
        fprintf(stderr,"Unable to register the SIGALRM signal\n");
        exit(1);
        }

        /*En caso que la cabecera del fichero log se tenga que poner solo una vez:*/
        /*Limpiamos la cadena*/
        strcpy(aux, "");
        strcpy(aux2, "");
        strcat(aux, hostname);
        strcat(aux, inet_ntoa(clientaddr_in.sin_addr)); //Metemos la IP
        strcat(aux, " ");
        strcat(aux, "UDP"); //Metemos protocolo de transporte
        strcat(aux, " ");
        snprintf(aux2, sizeof(aux2), "%d", clientaddr_in.sin_port);
        strcat(aux, aux2); //Metemos puerto efimero
        strcat(aux, " ");
        strcat(aux, ctime(&timevar)); //Metemos la fecha y hora

        /*Metemos en el archivo peticiones.log lo recibido*/
        if(NULL == (p = (fopen(registro, "a")))){

                fprintf(stderr, "No se ha podido abrir el fichero");

        }
        fputs(aux, p); //Ponemos en el fichero la cabecera
        fclose(p); //Cerramos el fichero

        do{
                socketON=1;
                /*TRATAMIENTO DEL MENSAJE*/
                strcpy(aux, ""); //Vaciamos la cadena                
                /*Troceamos el mensaje por \r\n*/
                q=0;
                cachos = strtok(envioCl, caracteresRetorno);
                while(cachos != NULL){

                        strcpy(vCachos[q], cachos);
                        cachos = strtok(NULL, caracteresRetorno);
                        q++;

                }
                /*Separamos el envio de la cabecera*/
                q=0;
                str = strtok(vCachos[0], " ");
                while(str != NULL){
        
                        strcpy(vCabecera[q], str);
                        /*Seguimos con el siguiente token*/
                        str = strtok(NULL, " ");
                        q++;

                }

                /*Troceamos el contenido del connection*/
                q=0;
                str = strtok(vCachos[2], " ");
                while(str != NULL){

                        strcpy(vConnection[q], str);
                        /*Seguimos con el siguiente token*/
                        str = strtok(NULL, " ");
                        q++;

                }

                /*Metemos el HTTP para el fichero .log*/
                strcpy(aux2, "");
                strcat(aux2, conexionRed);
                strcat(aux2, " ");

                strcpy(respuesta, "");
                /*Metemos el HTTP al mensaje*/
                strcpy(respuesta, conexionRed);
                strcat(respuesta, " ");

                /*Conseguimos la orden que manda el cliente, si no es GET se devolverá el error*/
                if(strcmp(vCabecera[0], "GET") != 0){
                        /*501 NOT IMPLEMENTED*/
                        /*RESPUESTA: */
                        strcat(respuesta, "501 Not Implemented");
                        strcat(respuesta, caracteresRetorno);

                        /*Metemos tambien la cabecera para el fichero .log*/
                        strcat(aux2, "501 Not Implemented");
                        strcat(aux2, caracteresRetorno);

                        /*SERVER: */
                        strcat(respuesta, "Server: ");
                        strcat(respuesta, hostname);
                        strcat(respuesta, caracteresRetorno);
                
                        /*CONNECTION: */
                        if(strcmp(vConnection[1], "keep-alive") == 0){
                                /*NO Se cierra la conexion*/
                                strcat(respuesta, "Connection: ");
                                strcat(respuesta, "keep-alive");
                                strcat(respuesta, caracteresRetorno);

                                /*FINAL: */
                                strcat(respuesta, caracteresRetorno);

                                /*HTML: */
                                strcat(respuesta, "<html><body><h1>501 Not Implemented</h1></body></html>\n");

                                sleep(1); //Tiempo de trabajo del servidor

                                /*Enviamos la respuesta al cliente*/
                                if (sendto(s, respuesta, BUFFERSIZE, 0, (struct sockaddr*) &clientaddr_in, addrlen) != BUFFERSIZE) errout(hostname);

                                /* Incrementamos el contador de peticiones */
                                reqcnt++;

                                /*Metemos en el .log*/
                                if(NULL == (p = (fopen(registro, "a")))){

                                        fprintf(stderr, "No se ha podido abrir el fichero");

                                }
                                fputs(aux2, p);
                                fclose(p);

                        }
                        else{
                                /*Se cierra la conexion*/
                                strcat(respuesta, "Connection: ");
                                strcat(respuesta, "close");
                                strcat(respuesta, caracteresRetorno);

                                /*FINAL: */
                                strcat(respuesta, caracteresRetorno);

                                /*HTML: */
                                strcat(respuesta, "<html><body><h1>501 Not Implemented</h1></body></html>\n");

                                sleep(1); //Tiempo de trabajo del servidor

                                /*Enviamos la respuesta al cliente*/
                                if (sendto(s, respuesta, BUFFERSIZE, 0, (struct sockaddr*) &clientaddr_in, addrlen) != BUFFERSIZE) errout(hostname);

                                /* Incrementamos el contador de peticiones */
                                reqcnt++;
                                /*Cerramos la conexion*/
                                close(s);
                                socketON=0;
                                /*Metemos en el .log*/
                                if(NULL == (p = (fopen(registro, "a")))){

                                        fprintf(stderr, "No se ha podido abrir el fichero");

                                }
                                fputs(aux2, p);
                                fclose(p);
                        }

                }
                else{ 
                        /*Si no existe el fichero en la carpeta devuelve null*/
                        strcpy(pathToFileHTML, pathToDirWeb);
                        strcat(pathToFileHTML, vCabecera[1]);
                        if(NULL == (html = (fopen(pathToFileHTML, "r")))){
                                 flaggie = 0;
                        }
                        else{
                                 flaggie = 1;
                        }

                        /*Si se ha encontrado el fichero (flaggie = true) se responde y se saca lo de dentro*/
                        if(flaggie){
                                /*200 OK*/
                                /*RESPUESTA: */
                                strcat(respuesta, "200 OK");
                                strcat(respuesta, caracteresRetorno);

                                /*Metemos tambien la cabecera para el fichero .log*/
                                strcat(aux2, "200 OK");
                                strcat(aux2, caracteresRetorno);


                                /*SERVER: */
                                strcat(respuesta, "Server: ");
                                strcat(respuesta, hostname);
                                strcat(respuesta, caracteresRetorno);

                                /*CONNECTION: */
                                if(strcmp(vConnection[1], "keep-alive") == 0){

                                        /*NO Se cierra la conexion*/
                                        strcat(respuesta, "Connection: ");
                                        strcat(respuesta, "keep-alive");
                                        strcat(respuesta, caracteresRetorno);

                                        /*FINAL: */
                                        strcat(respuesta, caracteresRetorno);

                                        /*HTML: */
                                        /*Vamos leyendo el fichero y almacenando en el apartado html de la estructura para mandarlo*/
                                        while(fgets(buf, BUFFERSIZE, html) != NULL){

                                                strcat(respuesta, buf);

                                        }
                                        fclose(html);

                                        sleep(1); //Tiempo de trabajo del servidor

                                        /*Enviamos la respuesta al cliente*/
                                        if (sendto(s, respuesta, BUFFERSIZE, 0, (struct sockaddr*) &clientaddr_in, addrlen) != BUFFERSIZE) errout(hostname);

                                        /* Incrementamos el contador de peticiones */
                                        reqcnt++;

                                        /*Metemos en el .log*/
                                        if(NULL == (p = (fopen(registro, "a")))){

                                                fprintf(stderr, "No se ha podido abrir el fichero");

                                        }
                                        fputs(aux2, p);
                                        fclose(p);

                                }
                                else{
                                        /*Se cierra la conexion*/
                                        strcat(respuesta, "Connection: ");
                                        strcat(respuesta, "close");
                                        strcat(respuesta, caracteresRetorno);

                                        /*FINAL: */
                                        strcat(respuesta, caracteresRetorno);

                                        /*HTML: */
                                        /*Vamos leyendo el fichero y almacenando en el apartado html de la estructura para mandarlo*/
                                        while(fgets(buf, BUFFERSIZE, html) != NULL){

                                                strcat(respuesta, buf);

                                        }
                                        fclose(html);

                                        sleep(1); //Tiempo de trabajo del servidor

                                        /*Enviamos la respuesta al cliente*/
                                        if (sendto(s, respuesta, BUFFERSIZE, 0, (struct sockaddr*) &clientaddr_in, addrlen) != BUFFERSIZE) errout(hostname);

                                        /* Incrementamos el contador de peticiones */
                                        reqcnt++;
                                        /*Cerramos la conexion*/
                                        close(s);
                                        socketON=0;
                                        /*Metemos en el .log*/
                                        if(NULL == (p = (fopen(registro, "a")))){

                                                fprintf(stderr, "No se ha podido abrir el fichero");

                                        }
                                        fputs(aux2, p);
                                        fclose(p);
                                }
                        }
                        else{
                                /*404 Not found*/
                                /*RESPUESTA: */
                                strcat(respuesta, "404 Not found");
                                strcat(respuesta, caracteresRetorno);

                                /*Metemos tambien la cabecera para el fichero .log*/
                                strcat(aux2, "404 Not found");
                                strcat(aux2, caracteresRetorno);

                                /*SERVER: */
                                strcat(respuesta, "Server: ");
                                strcat(respuesta, hostname);
                                strcat(respuesta, caracteresRetorno);

                                /*CONNECTION: */
                                if(strcmp(vConnection[1], "keep-alive") == 0){
                                        /*NO Se cierra la conexion*/
                                        strcat(respuesta, "Connection: ");
                                        strcat(respuesta, "keep-alive");
                                        strcat(respuesta, caracteresRetorno);

                                        /*FINAL: */
                                        strcat(respuesta, caracteresRetorno);

                                        /*HTML: */
                                        strcat(respuesta, "<html><body><h1>404 Not Found</h1></body></html>\n");

                                        sleep(1); //Tiempo de trabajo del servidor

                                        /*Enviamos la respuesta al cliente*/
                                        if (sendto(s, respuesta, BUFFERSIZE, 0, (struct sockaddr*) &clientaddr_in, addrlen) != BUFFERSIZE) errout(hostname);

                                        /* Incrementamos el contador de peticiones */
                                        reqcnt++;

                                        /*Metemos en el .log*/
                                        if(NULL == (p = (fopen(registro, "a")))){

                                                fprintf(stderr, "No se ha podido abrir el fichero");

                                        }
                                        fputs(aux2, p);
                                        fclose(p);

                                }
                                else{
                                        /*Se cierra la conexion*/
                                        strcat(respuesta, "Connection: ");
                                        strcat(respuesta, "close");
                                        strcat(respuesta, caracteresRetorno);

                                        /*FINAL: */
                                        strcat(respuesta, caracteresRetorno);

                                        /*HTML: */
                                        strcat(respuesta, "<html><body><h1>404 Not Found</h1></body></html>\n");

                                        sleep(1); //Tiempo de trabajo del servidor

                                        /*Enviamos la respuesta al cliente*/
                                        if (sendto(s, respuesta, BUFFERSIZE, 0, (struct sockaddr*) &clientaddr_in, addrlen) != BUFFERSIZE) errout(hostname);

                                        /* Incrementamos el contador de peticiones */
                                        reqcnt++;
                                        /*Cerramos la conexion*/
                                        close(s);
                                        socketON=0;
                                        /*Metemos en el .log*/
                                        if(NULL == (p = (fopen(registro, "a")))){
                                                fprintf(stderr, "No se ha podido abrir el fichero");
                                        }
                                        fputs(aux2, p);
                                        fclose(p);
                                }
                        }
                }

                if(socketON){
                        /*Recibir y volver a enviar*/
                        alarm(TIMEOUT);
                        memset(buffer, 0, sizeof(buf));
                        memset(envioCl, 0, sizeof(envioCl));
                        memset(vConnection, 0, sizeof(vConnection));
                        if (recvfrom (s, envioCl, BUFFERSIZE, 0, (struct sockaddr *) &clientaddr_in, &addrlen) == -1) {
                                if (errno == EINTR) {
                                        break;
                                } 
                                else{
                                        printf("Unable to get response from");
                                        exit(1);
                                }
                        }
                }
                else{
                        break;
                }
        }while(1);

        time (&timevar);
        printf("Completed port %u, %d requests, at %s\n", ntohs(clientaddr_in.sin_port), reqcnt, (char *) ctime(&timevar));
        close(s);
}

 

/*Señal para la señal de alarma*/
void handler(){

        printf("No hay mas mensajes que recibir\n");

}































