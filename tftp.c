#include "tftp.h"
// #define TFTP_TYPES


// Tableau de messages d'erreur correspondant aux codes d'erreur TFTP
const char* TFTPErrorMessages[NUM_TFTP_ERRORS] = {
    "Not defined, see error message (if any)",
    "File not found",
    "Access violation",
    "Disk full or allocation exceeded",
    "Illegal TFTP operation",
    "Unknown transfer ID",
    "File already exists",
    "No such user"
};




/**
 * \brief Envoie un paquet de données au client.
 * 
 * Construit un paquet de données TFTP avec le numéro de bloc spécifié
 * et les données fournies, puis l'envoie au client à l'adresse spécifiée.
 * 
 * \param sockfd Le descripteur de socket.
 * \param client_addr L'adresse du client.
 * \param block_number Le numéro de bloc du paquet de données.
 * \param data Les données à inclure dans le paquet.
 * \param data_size La taille des données.
 */
void send_data_packet(int sockfd, struct sockaddr_in* client_addr, uint16_t block_number, char *data, size_t data_size) {
    TFTP_DataPacket packet;
    packet.opcode = htons(TFTP_OPCODE_DATA); // Opcode 3 pour un paquet de données
    packet.block_number = htons(block_number); // Numéro de bloc (convertis en réseau)
    memcpy(packet.data, data, data_size); // Copier les données dans le paquet
    ssize_t bytes_sent = sendto(sockfd, &packet, data_size + TFTP_HEADER_SIZE, 0, (struct sockaddr *)client_addr, sizeof(*client_addr));
    if (bytes_sent == -1) {
        perror("Erreur lors de l'envoi du paquet de données");
        exit(EXIT_FAILURE);
    }
}



/**
 * \brief Envoie un paquet ACK (acknowledgment) au client.
 * 
 * Construit un paquet ACK TFTP avec le numéro de bloc spécifié
 * et l'envoie au client à l'adresse spécifiée.
 * 
 * \param sockfd Le descripteur de socket.
 * \param client_addr L'adresse du client.
 * \param block_number Le numéro de bloc du paquet ACK.
 */
void send_ack_packet(int sockfd, struct sockaddr_in *client_addr, uint16_t block_number) {
    TFTP_AckPacket ack_packet;
    ack_packet.opcode = htons(TFTP_OPCODE_ACK); // Opcode 4 pour un paquet ACK
    ack_packet.block_number = htons(block_number); // Numéro de bloc (converti en réseau)

    ssize_t bytes_sent = sendto(sockfd, &ack_packet, sizeof(ack_packet), 0, (struct sockaddr *)client_addr, sizeof(*client_addr));
    if (bytes_sent == -1) {
        perror("Erreur lors de l'envoi du paquet ACK");
        exit(EXIT_FAILURE);
    }
}




/**
 * \brief Envoie un paquet d'erreur au client.
 * 
 * Construit un paquet d'erreur TFTP avec le code d'erreur spécifié,
 * le message d'erreur et un message supplémentaire optionnel, puis
 * l'envoie au client à l'adresse spécifiée.
 * 
 * \param sockfd Le descripteur de socket.
 * \param client_addr L'adresse du client.
 * \param errorCode Le code d'erreur TFTP.
 * \param error_message Le message d'erreur.
 * \param additional_message Message supplémentaire optionnel.
 */
void send_error_packet(int sockfd, struct sockaddr_in* client_addr, uint16_t errorCode, const char* error_message, const char* additional_message) {
    TFTP_ErrorPacket errPacket;
    errPacket.opcode = htons(TFTP_OPCODE_ERR);
    errPacket.err_code = htons(errorCode);

    // Copier le message d'erreur dans le paquet d'erreur
    strncpy(errPacket.err_msg, error_message, MAX_ERROR_MSG_LEN);

    // Si additional_message n'est pas NULL et qu'il reste de la place dans le buffer, le concaténer
    if (additional_message != NULL && strlen(errPacket.err_msg) + strlen(additional_message) + 1 < MAX_ERROR_MSG_LEN) {
        strncat(errPacket.err_msg, ": ", MAX_ERROR_MSG_LEN - strlen(errPacket.err_msg) - 1);
        strncat(errPacket.err_msg, additional_message, MAX_ERROR_MSG_LEN - strlen(errPacket.err_msg) - 1);
    }

    // Envoyer le paquet d'erreur
    sendto(sockfd, &errPacket, sizeof(errPacket), 0, (struct sockaddr*)client_addr, sizeof(*client_addr));
}




/**
 * \brief Obtient le message d'erreur correspondant à un code d'erreur TFTP.
 * 
 * Retourne le message d'erreur correspondant au code d'erreur TFTP spécifié.
 * 
 * \param error_code Le code d'erreur TFTP.
 * \return Le message d'erreur correspondant au code d'erreur TFTP.
 */
const char* get_error_message(int error_code) {
    if (error_code >= 0 && error_code < NUM_TFTP_ERRORS) {
        return TFTPErrorMessages[error_code];
    } else {
        return "Unknown error";
    }
}




/**
 * \brief Crée un socket UDP et l'associe à une adresse IP et un port.
 * 
 * Crée un socket UDP et l'associe à une adresse IP et un port spécifiés.
 * 
 * \param ip L'adresse IP à utiliser (NULL pour utiliser l'adresse "any").
 * \param port Le numéro de port à utiliser.
 * \return Le descripteur de socket, ou -1 en cas d'erreur.
 */
int createUDPSocket(const char *ip, int port) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);// Création du socket UDP
    if (sockfd < 0) {
        perror("Erreur lors de la création du socket");
        return -1;
    }

    // Configuration de l'adresse IP et du port
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));


    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    if (ip == NULL) {
        server_addr.sin_addr.s_addr = htonl(INADDR_ANY); 
        // printf("any\n");
    } else {
        inet_pton(AF_INET, ip, &server_addr.sin_addr);
    }

    // Liaison du socket à l'adresse et au port spécifiés
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Erreur lors de la liaison du socket à l'adresse");
        close(sockfd);
        return -1;
    }
    // printf("Le serveur écoute sur l'adresse IP %s et le port %d\n", ip ? ip : INADDR_ANY, port);
    return sockfd;
}



/**
 * Cette fonction génère un nom de fichier temporaire en ajoutant l'extension ".tmp" au nom du fichier original.
 * @param nom_fichier Le nom du fichier original.
 * @return Un pointeur vers une nouvelle chaîne de caractères contenant le nom du fichier temporaire.
 *         Il est de la responsabilité de l'appelant de libérer la mémoire allouée après usage.
 */


char* get_temp_file_name(const char* nom_fichier) {
    char* nouveau_nom = malloc(strlen(nom_fichier) + 5); // +5 pour ".tmp\0"

    // Vérifier si l'allocation de mémoire a réussi
    if (nouveau_nom == NULL) {
        perror("Erreur lors de l'allocation de mémoire");
        return NULL;
    }
    // Copier le nom du fichier original dans le nouveau nom
    strcpy(nouveau_nom, nom_fichier);
    strcat(nouveau_nom, ".tmp");// Ajouter l'extension ".tmp"
    return nouveau_nom;

}


/**
 * Cette fonction supprime fichier temporaire d'un fichier original.
 * @param nom_fichier Le nom du fichier original.
 * @return 0 sucess        -1 failed
 */
int remove_tempfile(const char* nom_fichier) {
    char* temp_filename = get_temp_file_name(nom_fichier);
        if (access(temp_filename, F_OK) != -1) {
            // printf("Le fichier existe.\n");
            if (remove(temp_filename) != 0) {
                perror("Erreur lors de la suppression de l'ancien fichier");
                return -1;
            }
        }
        free(temp_filename);
        return 0;
}