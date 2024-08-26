/* 
   TFTP (Trivial File Transfer Protocol) - Définitions et structures de données
*/


#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <string.h>

#ifndef TFTP_TYPES
#define TFTP_TYPES


// Définition des tailles maximales pour les paquets TFTP
#define MAX_PACKET_SIZE 516
#define MAX_DATA_SIZE 512
#define TFTP_HEADER_SIZE 4
#define MAX_ERROR_MSG_LEN 512


// Codes d'opération TFTP
#define TFTP_OPCODE_RRQ 1
#define TFTP_OPCODE_WRQ 2
#define TFTP_OPCODE_DATA 3
#define TFTP_OPCODE_ACK 4
#define TFTP_OPCODE_ERR 5


// Paramètres de temporisation et de réessa
#define TIMEOUT_SECONDS 1
#define MAX_RETRIES 3


typedef struct {
    uint16_t opcode;    // Code d'opération TFTP (RRQ ou WRQ)
    char filename[504]; // Nom du fichier à transférer
    char mode[10]; // Mode de transfert (octet, netascii)
} TFTP_Request;

typedef struct {
    uint16_t opcode;
    uint16_t block_number;
    char data[MAX_DATA_SIZE]; // Données du paquet
} TFTP_DataPacket;

typedef struct {
    uint16_t opcode;
    uint16_t block_number;
} TFTP_AckPacket;

typedef struct {
    uint16_t opcode;
    uint16_t err_code;
    char err_msg[MAX_ERROR_MSG_LEN];
} TFTP_ErrorPacket;



// Enumération des codes d'erreur TFTP
typedef enum TFTPError {
    NotDefined = 0,
    FileNotFound = 1,
    AccessViolation = 2,
    DiskFullOrAllocationExceeded = 3,
    IllegalOperation = 4,
    UnknownTransferID = 5,
    FileAlreadyExists = 6,
    NoSuchUser = 7,
    NUM_TFTP_ERRORS 
}TFTPError;



typedef union {
    TFTP_DataPacket data_packet;
    TFTP_AckPacket ack_packet;
} Packet;



// Enumération pour indiquer le type de paquet (soit un paquet de données, soit un paquet d'acquittement)
typedef enum {
    DATA_PACKET,
    ACK_PACKET
} PacketType;





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
void send_data_packet(int sockfd, struct sockaddr_in* client_addr, uint16_t block_number, char *data, size_t data_size);




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
void send_ack_packet(int sockfd, struct sockaddr_in *client_addr, uint16_t block_number);



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
void send_error_packet(int sockfd, struct sockaddr_in* client_addr, uint16_t errorCode, const char* error_message, const char* additional_message);


/**
 * \brief Obtient le message d'erreur correspondant à un code d'erreur TFTP.
 * 
 * Retourne le message d'erreur correspondant au code d'erreur TFTP spécifié.
 * 
 * \param error_code Le code d'erreur TFTP.
 * \return Le message d'erreur correspondant au code d'erreur TFTP.
 */
const char* get_error_message(int error_code);



/**
 * \brief Crée un socket UDP et l'associe à une adresse IP et un port.
 * 
 * Crée un socket UDP et l'associe à une adresse IP et un port spécifiés.
 * 
 * \param ip L'adresse IP à utiliser (NULL pour utiliser l'adresse "any").
 * \param port Le numéro de port à utiliser.
 * \return Le descripteur de socket, ou -1 en cas d'erreur.
 */
int createUDPSocket(const char *ip, int port);




/**
 * Cette fonction génère un nom de fichier temporaire en ajoutant l'extension ".tmp" au nom du fichier original.
 * @param nom_fichier Le nom du fichier original.
 * @return Un pointeur vers une nouvelle chaîne de caractères contenant le nom du fichier temporaire.
 *         Il est de la responsabilité de l'appelant de libérer la mémoire allouée après usage.
 */
char* get_temp_file_name(const char* nom_fichier);



/**
 * Cette fonction supprime fichier temporaire d'un fichier original.
 * @param nom_fichier Le nom du fichier original.
 * @return 0 sucess        -1 failed
 */
int remove_tempfile(const char* nom_fichier);

#endif



