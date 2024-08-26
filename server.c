#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <strings.h>

#include "tftp.h"
#include "sync.h"


#define SERVER_MAIN_PORT 69

#define SELECT_TIMEOUT_SEC 1
#define TIMEOUT_SEC 4


// type def

typedef struct {
    int sockfd;
    struct sockaddr_in addr;
    socklen_t len;
    TFTP_Request request;
    FILE* file_fd;
    char buffer[MAX_DATA_SIZE];
    int buffer_size;
    uint16_t block_number;
    struct timeval last_sent_time; // Temps du dernier envoi de paquet
    PacketType last_action_type; // Type de la dernière action effectuée (paquet de données ou paquet d'acquittement)
    int retries; // Nombre de tentatives de retransmission
} ClientInfo;

typedef void (*TFTP_HandlerFunction)(ClientInfo* client);

// fun def
void initialize_Client(ClientInfo* client);
void add_client(ClientInfo *client, int sockfd);
void delete_client(int sockfd);
void handle_new_read_request(ClientInfo *client);
void handle_new_write_request(ClientInfo *client);
void update_maxfd();

double elapsed_time(struct timeval *start, struct timeval *end);
void check_timeouts_and_retransmit();




// global var
ClientInfo* clients[FD_SETSIZE];
int maxfd;
int num_clients = 0;
fd_set readfds;
int server_sockfd;
ServerFileArray fileArray;







int main() {
    struct sockaddr_in cliaddr;
    socklen_t len;
    char buffer[MAX_PACKET_SIZE];
    
    // Création socket
    server_sockfd = createUDPSocket(NULL,SERVER_MAIN_PORT);

    if (server_sockfd < 0){
        printf("err création main socket !\n");
        exit(EXIT_FAILURE);
    }

    printf("server init (fd_setsize %d)\n",FD_SETSIZE);
    printf("Serveur TFTP en attente de connexions sur le port %d...\n",SERVER_MAIN_PORT);

    // Server Is Working
    memset(&cliaddr, 0, sizeof(cliaddr));
    initialize_serverFileArray(&fileArray);

    FD_ZERO(&readfds);
    FD_SET(server_sockfd, &readfds);
    maxfd = server_sockfd;

    long size_in_bytes, size_in_mb,size_in_kb;
    // Configuration du timeout pour select
    struct timeval timeout;
    struct timeval *timeout_pointer;

    // boucle principal
    while (1) {
        
        if (num_clients > 0){
            timeout.tv_sec = SELECT_TIMEOUT_SEC;
            timeout.tv_usec = 0;
            timeout_pointer = &timeout;
            check_timeouts_and_retransmit();
        }else {
            timeout_pointer = NULL;
        }
        
        fd_set tmpfds = readfds;
        int activity = select(maxfd+1, &tmpfds, NULL, NULL, timeout_pointer);

        // Vérification si select a renvoyé une erreur ou s'il n'y a eu aucune activité
        if (activity < 0) {
            perror("select error");
            exit(EXIT_FAILURE);
        } else if (activity == 0) {
            // Aucune activité sur les sockets, timeout atteint
            // printf("Time Out !\n");
            continue;
        }

        if (FD_ISSET(server_sockfd, &tmpfds)) {
            len = sizeof(cliaddr);

            // Receive message from client
            int bytes_received = recvfrom(server_sockfd, (char *)buffer, MAX_PACKET_SIZE,0,(struct sockaddr *)&cliaddr, &len);
            if (bytes_received == -1) {
                perror("Erreur lors de la réception des données");
                exit(EXIT_FAILURE);
            }

            // printf("Taille du paquet reçu: %zd octets\n", bytes_received);
            ClientInfo* client = (ClientInfo *)malloc(sizeof(ClientInfo));
            initialize_Client(client);

            // Récupérer et afficher l'adresse du client
            char client_ip_address[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &cliaddr.sin_addr, client_ip_address, INET_ADDRSTRLEN);
            // printf("Adresse du client: %s\n", client_ip_address);

            
            int newsockfd = createUDPSocket(client_ip_address,0); // Create a new socket with ephemeral port for responding to client
            if (newsockfd < 0){
                perror("socket creation failed");
            }
            // ajouet le client
            add_client(client,newsockfd);

            client->sockfd = newsockfd;
            memcpy(&client->addr,&cliaddr,sizeof(cliaddr));
            client->len = len;
            

            // remplissage et verification des info

            memcpy(&client->request.opcode, buffer, sizeof(uint16_t));
            TFTP_HandlerFunction selectedHandler = NULL;

            // Gestion de la demande en fonction de l'opcode
            if (ntohs(client->request.opcode) == TFTP_OPCODE_RRQ) {
                client->block_number = 1;
                selectedHandler = handle_new_read_request;
            } else if (ntohs(client->request.opcode) == TFTP_OPCODE_WRQ) {
                client->block_number = 0;
                selectedHandler = handle_new_write_request;
            } else {
                // Opcode non pris en charge, envoi d'un paquet d'erreur au client
                send_error_packet(client->sockfd,&client->addr,NotDefined,get_error_message(NotDefined),"Opcode non pris en charge");
                delete_client(newsockfd);
                continue;
            }

            // Extraction du nom de fichier
            size_t filename_length = strlen(buffer + 2);
            strcpy(client->request.filename, buffer + 2);
            if (filename_length == 0) {
                // Gestion de l'erreur : Nom de fichier vide
                printf("Client[%d] : Erreur! Nom de fichier vide.\n",client->sockfd);
                // Envoyer un paquet d'erreur au client
                send_error_packet(client->sockfd,&client->addr,NotDefined,get_error_message(NotDefined),"Nom de fichier vide");
                delete_client(newsockfd);
                continue;
            }

            // Extraction du mode de transfert
            size_t mode_offset = 2 + filename_length + 1; // Offset pour accéder au début du mode
            size_t mode_length = strlen(buffer + mode_offset);

            strcpy(client->request.mode, buffer + mode_offset);
            if (mode_length == 0 || (strcasecmp(client->request.mode, "netascii") != 0 && strcasecmp(client->request.mode, "octet") != 0) ) {
                // Gestion de l'erreur : Mode de transfert non reconnu
                printf("Erreur: Mode de transfert non reconnu.\n");
                send_error_packet(client->sockfd,&client->addr,NotDefined,get_error_message(NotDefined),"Mode de transfert non reconnu");
                delete_client(newsockfd);
                continue;
            }

            // RRQ | WRQ
            selectedHandler(client);
        }

        // Check if any clients are responding
        for (int i = server_sockfd + 1; i <= maxfd; ++i) {
            if (clients[i] != NULL && FD_ISSET(i, &tmpfds)) {

                // int bytes_received = recvfrom(i, (char *)clients[i]->buffer, MAX_PACKET_SIZE, 0, (struct sockaddr *)&clients[i]->addr, &clients[i]->len);
                int bytes_received = recvfrom(i, (char *)clients[i]->buffer, MAX_PACKET_SIZE, 0, (struct sockaddr *)&cliaddr, &len);
                if (bytes_received == -1) {
                    perror("Erreur lors de la réception des données du client");
                    // Gérer l'erreur (par exemple, fermer la connexion avec le client)
                    continue;
                }

                if (clients[i]->addr.sin_addr.s_addr != cliaddr.sin_addr.s_addr || clients[i]->addr.sin_port != cliaddr.sin_port){
                    send_error_packet(server_sockfd, &cliaddr,UnknownTransferID,get_error_message(UnknownTransferID),NULL);
                    continue;
                }

                
                // verifier le code operation du packet recu
                uint16_t opcode;
                memcpy(&opcode, clients[i]->buffer, sizeof(uint16_t));
                opcode = ntohs(opcode);

                if (opcode == TFTP_OPCODE_ACK){
                    
                    // continuer un RRQ
                    
                    if (bytes_received >= 4) {
                        uint16_t block_number;
                        memcpy(&block_number, clients[i]->buffer + 2, sizeof(uint16_t));
                        block_number = ntohs(block_number);

                        
                        // Vérifiez si le numéro de bloc correspond au numéro attendu
                        if (block_number == clients[i]->block_number) {
                            // printf("[OLD] Client[%d] (bloc_num : %d) : \n",i, clients[i]->sockfd, clients[i]->block_number);
                            
                            if (clients[i]->buffer_size < MAX_DATA_SIZE){
                                stop_file_session(clients[i]->request.filename,READ_MODE,&fileArray);
                                size_in_bytes = ftell(clients[i]->file_fd);
                                size_in_kb = size_in_bytes / 1024;
                                size_in_mb = size_in_bytes / (1024 * 1024);
                                printf("Client[%d] ^_^ Transmission terminée avec succès, total: %ld Mo (%ld Ko)\n",i, size_in_mb, size_in_kb);
                                delete_client(i);
                                continue;
                            }

                            clients[i]->block_number++;
                            clients[i]->buffer_size = fread(clients[i]->buffer, 1, sizeof(clients[i]->buffer), clients[i]->file_fd);
                            // Envoyer le paquet de données au client
                            send_data_packet(clients[i]->sockfd, &clients[i]->addr, clients[i]->block_number, clients[i]->buffer,  clients[i]->buffer_size);
                            clients[i]->last_action_type = DATA_PACKET;
                            gettimeofday(&(clients[i]->last_sent_time), NULL);
                            // printf("last_sent_time: %ld seconds, %ld microseconds\n", clients[i]->last_sent_time.tv_sec, clients[i]->last_sent_time.tv_usec);


                        } else {
                            // Gérer le cas où un ACK incorrect est reçu
                            printf("Client[%d] : ACK incorrect reçu pour le bloc %d (attendu: %d)\n",i, block_number, clients[i]->block_number);
                        }
                    } else {
                        // le cas où le paquet ACK reçu est trop court pour contenir le numéro de bloc
                        printf("Paquet ACK invalide reçu (taille insuffisante)\n");
                        send_error_packet(clients[i]->sockfd,&clients[i]->addr,IllegalOperation,get_error_message(IllegalOperation),NULL);
                        delete_client(i);
                        continue;
                    }

                } else if (opcode == TFTP_OPCODE_DATA) {

                    // continuer un WRQ

                    uint16_t block_number;
                    memcpy(&block_number, clients[i]->buffer + 2, sizeof(uint16_t));
                    block_number = ntohs(block_number);

                    
                    if (block_number == clients[i]->block_number){
                        size_t bytesWritten = fwrite(clients[i]->buffer + TFTP_HEADER_SIZE, 1, bytes_received - TFTP_HEADER_SIZE, clients[i]->file_fd);
                        
                        if ((int) bytesWritten < bytes_received - TFTP_HEADER_SIZE) {
                            printf("Erreur lors de l'écriture dans le fichier\n");
                            // Envoi d'un paquet d'erreur au client
                            send_error_packet(clients[i]->sockfd,&clients[i]->addr,DiskFullOrAllocationExceeded,get_error_message(DiskFullOrAllocationExceeded),NULL);
                            remove_tempfile(clients[i]->request.filename);
                            delete_client(i);
                            exit(EXIT_FAILURE);
                        }
                        
                        send_ack_packet(clients[i]->sockfd,&clients[i]->addr,clients[i]->block_number);
                        clients[i]->last_action_type = ACK_PACKET;
                        gettimeofday(&(clients[i]->last_sent_time), NULL);
                    } else if(block_number == clients[i]->block_number-1) {
                        send_ack_packet(clients[i]->sockfd,&clients[i]->addr,clients[i]->block_number);
                        clients[i]->last_action_type = ACK_PACKET;
                        gettimeofday(&(clients[i]->last_sent_time), NULL);
                    } else {
                        send_error_packet(clients[i]->sockfd,&clients[i]->addr,NotDefined,get_error_message(NotDefined),NULL);
                        remove_tempfile(clients[i]->request.filename);
                        delete_client(i);
                        continue;
                    }
                    

                    if (bytes_received < MAX_PACKET_SIZE){
                        // c'est le dernier packet

                        // Supprimer l'ancien fichier
                        if (access(clients[i]->request.filename, F_OK) != -1) {
                            // printf("Le fichier existe.\n");
                            if (remove(clients[i]->request.filename) != 0) {
                                perror("Erreur lors de la suppression de l'ancien fichier");
                            }
                        }

                        // Renommer le fichier temporaire en cas de succès
                        if (rename(get_temp_file_name(clients[i]->request.filename), clients[i]->request.filename) != 0) {
                            perror("Erreur lors du renommage du fichier temporaire");
                        }

                        remove_tempfile(clients[i]->request.filename);
                        
                        stop_file_session(clients[i]->request.filename,WRITE_MODE,&fileArray);
                        size_in_bytes = ftell(clients[i]->file_fd);
                        size_in_kb = size_in_bytes / 1024;
                        size_in_mb = size_in_bytes / (1024 * 1024);

                        printf("Client[%d] ^_^ Réception terminée avec succès. total: %ld Mo (%ld Ko)\n",i, size_in_mb, size_in_kb);
                        delete_client(i);
                        continue;
                    }

                    clients[i]->block_number++;
                } else if (opcode == TFTP_OPCODE_ERR){
                    remove_tempfile(clients[i]->request.filename);
                    delete_client(i);
                    continue;
                }else {
                    send_error_packet(clients[i]->sockfd,&clients[i]->addr,IllegalOperation,get_error_message(IllegalOperation),NULL);
                    remove_tempfile(clients[i]->request.filename);
                    delete_client(i);
                    continue;
                }

            }
        }
    }

    close(server_sockfd);
    return 0;
}



/**
 * Ajoute un nouveau client au serveur.
 * 
 * Cette fonction ajoute un nouveau client au serveur en associant son descripteur de fichier à la structure
 * ClientInfo correspondante, en l'ajoutant au set de descripteurs de fichiers à surveiller et en mettant à
 * jour la valeur de maxfd si nécessaire.
 * 
 * @param client Un pointeur vers la structure ClientInfo représentant le nouveau client.
 * @param sockfd Le descripteur de fichier associé au nouveau client.
 */
void add_client(ClientInfo *client, int sockfd) {
        clients[sockfd] = client;
        
        FD_SET(sockfd, &readfds);// Add the new socket to the set of sockets to monitor
        if (sockfd > maxfd) {
            maxfd = sockfd;
        }
        num_clients++;
        // printf("Client[%d] Ajouté\n",sockfd);
}




/**
 * Supprime un client du serveur.
 * 
 * Cette fonction supprime un client du serveur en retirant son descripteur de fichier du
 * set de descripteurs de fichiers à surveiller, en fermant le socket du client et en libérant
 * la mémoire allouée pour la structure ClientInfo correspondante.
 * 
 * @param sockfd Le descripteur de fichier du client à supprimer.
 */
void delete_client(int sockfd) {
        FD_CLR(sockfd, &readfds); // Retirer le socket du set de sockets à surveiller
        close(sockfd); // Fermer le socket du client
        if (clients[sockfd]->file_fd !=NULL){
            fclose(clients[sockfd]->file_fd);
        }
        free(clients[sockfd]);
        clients[sockfd] = NULL;
        maxfd--;
        num_clients--;
        // printf("Client[%d] removed.\n", sockfd);
        update_maxfd();
}








/**
 * Gère une nouvelle demande de lecture (RRQ) du client.
 * 
 * Cette fonction traite une demande de lecture reçue du client. Elle vérifie l'existence du fichier demandé,
 * les autorisations de lecture sur ce fichier, et s'il est actuellement en cours d'accès par un autre client.
 * Ensuite, elle ouvre le fichier en mode lecture binaire ou texte selon le mode de transfert spécifié, lit les
 * données du fichier dans un tampon, envoie un paquet de données contenant les données lues au client, et met à jour
 * les informations sur le dernier envoi.
 * 
 * @param client Le pointeur vers la structure ClientInfo représentant le client ayant envoyé la demande.
 */
void handle_new_read_request(ClientInfo *client) {

    printf("New Client[%d] | %s | %s | %s\n",client->sockfd,"RRQ",client->request.filename,client->request.mode);

    if (access(client->request.filename, F_OK) == -1) {
        printf("Client[%d] : file Not Found\n",client->sockfd);
        send_error_packet(client->sockfd,&client->addr,FileNotFound,get_error_message(FileNotFound),NULL);
        delete_client(client->sockfd);
        return;
    } 


    if (access(client->request.filename, R_OK ) == -1) {
        printf("Client[%d] : Permission denied reading\n",client->sockfd);
        send_error_packet(client->sockfd,&client->addr,AccessViolation,get_error_message(AccessViolation),NULL);
        delete_client(client->sockfd);
        return;
    }

    if (start_file_session(client->request.filename,READ_MODE,&fileArray) !=0) {
        printf("Client[%d] : Error! The file is currently being accessed by another client.\n", client->sockfd);
        send_error_packet(client->sockfd,&client->addr,NotDefined,"The file is currently in use !",NULL);
        delete_client(client->sockfd);
        return;
    }
    

    // Vérifier le mode de transfert (netascii ou octet)
    if (strcasecmp(client->request.mode, "netascii") == 0 ) {
        client->file_fd = fopen(client->request.filename, "r");
    } else if (strcasecmp(client->request.mode, "octet") == 0) {
       client->file_fd = fopen(client->request.filename, "rb");
    } else {
        // Mode de transfert non pris en charge, envoyer un paquet d'erreur au client
        send_error_packet(client->sockfd,&client->addr,IllegalOperation,get_error_message(IllegalOperation),NULL);
        printf("Client[%d] : Mode de transfert non pris en charge",client->sockfd);
        delete_client(client->sockfd);
        return;
    }

    if (client->file_fd == NULL) {
        // En cas d'erreur lors de l'ouverture du fichier, envoyer un paquet d'erreur au client
        send_error_packet(client->sockfd,&client->addr,NotDefined,get_error_message(NotDefined),NULL);
        perror("Erreur lors de l'ouverture du fichier en lecture");
        delete_client(client->sockfd);
        return;
    }
    maxfd++;

    client->buffer_size = fread(client->buffer, 1, MAX_DATA_SIZE, client->file_fd);
    send_data_packet(client->sockfd, &client->addr, client->block_number, client->buffer, client->buffer_size);
    client->last_action_type = DATA_PACKET;
    gettimeofday(&(client->last_sent_time), NULL);
    // printf("data %d sent taille %d\n",client->block_number,client->buffer_size);
}





/**
 * Gère une nouvelle demande d'écriture (WRQ) du client.
 * 
 * Cette fonction traite une demande d'écriture reçue du client. Elle vérifie les autorisations d'écriture
 * sur le fichier demandé, vérifie si le fichier est actuellement en cours d'accès par un autre client,
 * ouvre le fichier en mode écriture binaire ou texte selon le mode de transfert spécifié, et envoie un paquet
 * d'acquittement pour confirmer le début de la transmission.
 * 
 * @param client Le pointeur vers la structure ClientInfo représentant le client ayant envoyé la demande.
 */
void handle_new_write_request(ClientInfo *client) {
    printf("New Client[%d] | %s | %s | %s\n",client->sockfd,"WRQ",client->request.filename,client->request.mode);

    // // Vérifie si vous avez le droit de lire et d'écrire dans le fichier
    // if (access(client->request.filename,W_OK) == -1) {
    //     printf("Client[%d] : Permission denied writing\n",client->sockfd);
    //     send_error_packet(client->sockfd,&client->addr,AccessViolation,get_error_message(AccessViolation),NULL);
    //     delete_client(client->sockfd);
    //     return;
    // }

    if (start_file_session(client->request.filename,WRITE_MODE,&fileArray) != 0) {
        printf("Client[%d] : Error! The file is currently being accessed by another client.\n", client->sockfd);
        send_error_packet(client->sockfd,&client->addr,NotDefined,"The file is currently in use !",NULL);
        delete_client(client->sockfd);
        return;
    }
    


    char* temp_filename = get_temp_file_name(client->request.filename);
    // Vérifier le mode de transfert (netascii ou octet)
    if (strcasecmp(client->request.mode, "netascii") == 0 ) {
        client->file_fd = fopen(temp_filename, "w");
    } else if (strcasecmp(client->request.mode, "octet") == 0) {
       client->file_fd = fopen(temp_filename, "wb");
    } else {
        // Mode de transfert non pris en charge, envoyer un paquet d'erreur au client
        send_error_packet(client->sockfd,&client->addr,IllegalOperation,get_error_message(IllegalOperation),NULL);
        printf("Client[%d] : Mode de transfert non pris en charge",client->sockfd);
        delete_client(client->sockfd);
        return;
    }
    free(temp_filename);

    if (client->file_fd == NULL) {
        // En cas d'erreur lors de l'ouverture du fichier, envoyer un paquet d'erreur au client
        send_error_packet(client->sockfd,&client->addr,NotDefined,get_error_message(NotDefined),NULL);
        perror("Erreur lors de l'ouverture du fichier en lecture");
        delete_client(client->sockfd);
        return;
    }

    
    maxfd++;    
   
    // Envoie un paquet d'acquittement pour confirmer le début de la transmission
    send_ack_packet(client->sockfd,&client->addr,client->block_number);
    client->last_action_type = ACK_PACKET;
    gettimeofday(&(client->last_sent_time), NULL);
    client->block_number = 1;
}





/**
 * Met à jour la valeur de maxfd en recherchant le plus grand descripteur de fichier ouvert.
 * 
 * maxfd est utilisé pour déterminer le nombre maximal de descripteurs de fichier à surveiller
 * lors de l'utilisation de fonctions telles que select() ou poll().
 */
void update_maxfd(){
    maxfd = server_sockfd;
    for (int fd = 0; fd < 1024; fd++) {
    // Vérifier si le descripteur de fichier est ouvert
        if (fcntl(fd, F_GETFD) != -1 || errno != EBADF) {
            // printf("Descripteur de fichier ouvert : %d\n", fd);
            if (fd > maxfd){
                maxfd = fd;
            }
        }
    }
}





/**
 * Initialise une structure ClientInfo.
 * 
 * @param client Un pointeur vers la structure ClientInfo à initialiser.
 */
void initialize_Client(ClientInfo* client) {
    
    client->sockfd = -1; // Initialize sockfd to -1 (invalid value)
    client->len = sizeof(client->addr); // Initialize len to the size of addr
    client->file_fd = NULL; // Initialize file_fd to NULL (no file open)
    client->buffer_size = 0; // Initialize buffer_size to 0
    client->block_number = 0; // Initialize block_number to 0
    // Clear memory for sockaddr_in
    memset(&client->addr, 0, sizeof(client->addr));
    // Clear memory for TFTP_Request
    memset(&client->request, 0, sizeof(client->request));
    // Set default values for TFTP_Request
    client->request.opcode = 0; // Set opcode to 0
    strcpy(client->request.filename, ""); // Set filename to an empty string
    strcpy(client->request.mode, ""); // Set mode to an empty string
    gettimeofday(&(client->last_sent_time), NULL);
    client->retries = 0;
    client->last_action_type = (PacketType) NULL;

}






/**
 * @brief Vérifie les délais d'attente expirés et retransmet les paquets si nécessaire.
 * 
 * Parcourt tous les clients et vérifie si le délai d'attente pour la retransmission des paquets a expiré.
 * Si le délai est dépassé, les paquets précédemment envoyés sont retransmis.
 * Cette fonction est appelée périodiquement pour gérer les retransmissions en cas de délais d'attente expirés.
 */
void check_timeouts_and_retransmit() {
    struct timeval now;
    gettimeofday(&now, NULL);

    for (int i = 0; i <= maxfd; ++i) {
        if (clients[i] != NULL) {
            double elapsed = elapsed_time(&(clients[i]->last_sent_time), &now);
            if (elapsed >= TIMEOUT_SEC) {
                // Retransmettre le dernier paquet envoyé
                if (clients[i]->last_action_type == DATA_PACKET) {
                    // Si le dernier paquet envoyé était un paquet de données, retransmettre ce paquet
                    send_data_packet(clients[i]->sockfd, &(clients[i]->addr), clients[i]->block_number, clients[i]->buffer, clients[i]->buffer_size);
                    printf("Client[%d] : Time Out ! retransmission DATA[%d]\n",i,clients[i]->block_number);
                } else if (clients[i]->last_action_type == ACK_PACKET) {
                    // Si le dernier paquet envoyé était un paquet d'acquittement, retransmettre ce paquet
                    send_ack_packet(clients[i]->sockfd, &(clients[i]->addr), clients[i]->block_number);
                    printf("Client[%d] : Time Out !  retransmission ACK[%d]\n",i,clients[i]->block_number);
                }
            
                gettimeofday(&(clients[i]->last_sent_time), NULL); // Mettre à jour le temps du dernier envoi
                clients[i]->retries++; // Incrémenter le nombre de tentatives de retransmission
                // Vérifier si le nombre de tentatives de retransmission a dépassé la limite
                if (clients[i]->retries >= MAX_RETRIES) {
                    printf("Client[%d] Nombre maximum de tentatives atteint\n",i);

                    if (clients[i]->last_action_type == DATA_PACKET) {
                        stop_file_session(clients[i]->request.filename,READ_MODE,&fileArray);
                    } else if (clients[i]->last_action_type == ACK_PACKET) {
                        remove_tempfile(clients[i]->request.filename);
                        stop_file_session(clients[i]->request.filename,WRITE_MODE,&fileArray);
                    }

                    delete_client(clients[i]->sockfd); // Supprimer le client s'il a dépassé la limite de retransmissions
                }

            }
        }
    }
}




/**
 * Calcul du temps écoulé en secondes entre deux instants donnés.
 * 
 * @param start Pointeur vers la structure timeval représentant le début du laps de temps.
 * @param end Pointeur vers la structure timeval représentant la fin du laps de temps.
 * @return Le temps écoulé en secondes sous forme de double.
 */
double elapsed_time(struct timeval *start, struct timeval *end) {
    return (end->tv_sec - start->tv_sec) + (end->tv_usec - start->tv_usec) / 1000000.0;
}