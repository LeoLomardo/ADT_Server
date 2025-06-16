#include "server.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

void send_hl7_message(int client_sock, const char* message) {
    int message_len = strlen(message);
    int total_len = message_len + 3; 

    char envelope[total_len];

    //documentacao formato ensagem q tem q enviar
    envelope[0] = 0x0B;                         // VT (início)
    memcpy(&envelope[1], message, message_len); // mensagem HL7
    envelope[message_len + 1] = 0x1C;           // FS
    envelope[message_len + 2] = 0x0D;           // CR

    printf("Pacote HL7 enviado (hex): ");
    for (int i = 0; i < total_len; i++) {
        printf("%02X ", (unsigned char)envelope[i]);
    }
    printf("\n");

    send(client_sock, envelope, total_len, 0);
}

void start_hl7_server() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("Socket failed");
        return;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr(IP_ADDRESS);
    memset(server_addr.sin_zero, '\0', sizeof server_addr.sin_zero);

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_sock);
        return;
    }

    if (listen(server_sock, 5) < 0) {
        perror("Listen failed");
        close(server_sock);
        return;
    }

    printf("HL7 MLLP Server rodando em %s:%d\n", IP_ADDRESS, PORT);

    while (1) {
        addr_size = sizeof client_addr;
        client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &addr_size);

        if (client_sock < 0) {
            perror("Accept failed");
            continue;
        }

        printf("Conexão estabelecida: %s\n", inet_ntoa(client_addr.sin_addr));

        FILE* file = fopen("mensagens/adt_patient.hl7", "r");
        if (!file) {
            perror("Falha ao abrir mensagem HL7");
            close(client_sock);
            continue;
        }

        char message[2048];
        size_t read_size = fread(message, 1,sizeof(message)-1, file);
        message[read_size] ='\0'; // Null-terminate the string
        fclose(file);

        send_hl7_message(client_sock, message);
        // printf("Enviando mensagem HL7:\n%s\n", message);
        printf("Mensagem HL7 enviada!\n");

        close(client_sock);
    }
}
