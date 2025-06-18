
#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define CERT_FILE "cert.pem"
#define KEY_FILE "key.pem"
#define MAX_MSG_SIZE 4096

const char* adt_response = 
"MSH|^~\\&|MINDRAY_ADT_GATEWAY|HOSPITAL|MINDRAY_N_SERIES|N15|20250618170000||ADT^A01|MSG00001|P|2.3.1\r"
"EVN|A01|20250618170000\r"
"PID|1||123456789^^^HOSPITAL&1.2.3.4.5.6.7.8.9&ISO^MR||SILVA^JOAO^A||19800101|M|||RUA UM 123^^RIO DE JANEIRO^RJ^20000-000^BR||(21)99999-9999\r"
"PV1|1|I|CARDIO^201^3B^HOSPITAL||||12345^MENDES^CARLOS^A^^DR|||||||||||||||||||||||||||202506181650\r";

void initialize_openssl() {
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
}

void cleanup_openssl() {
    EVP_cleanup();
}

SSL_CTX* create_context() {
    const SSL_METHOD *method = TLS_server_method();
    SSL_CTX *ctx = SSL_CTX_new(method);
    if (!ctx) {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    return ctx;
}

void configure_context(SSL_CTX *ctx) {
    if (SSL_CTX_use_certificate_file(ctx, CERT_FILE, SSL_FILETYPE_PEM) <= 0 ||
        SSL_CTX_use_PrivateKey_file(ctx, KEY_FILE, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
}

void send_mllp_message(SSL *ssl, const char *message) {
    int len = strlen(message);
    char envelope[len + 3];
    envelope[0] = 0x0B;
    memcpy(&envelope[1], message, len);
    envelope[len + 1] = 0x1C;
    envelope[len + 2] = 0x0D;

    SSL_write(ssl, envelope, len + 3);
    printf("Mensagem ADT^A01 enviada ao monitor.\n");
}

void start_hl7_server() {
    initialize_openssl();
    SSL_CTX *ctx = create_context();
    configure_context(ctx);

    int sockfd;
    struct sockaddr_in addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = inet_addr(IP_ADDRESS);

    bind(sockfd, (struct sockaddr*)&addr, sizeof(addr));
    listen(sockfd, 1);

    printf("Servidor ADT com TLS escutando em %s:%d...\n", IP_ADDRESS, PORT);

    while (1) {
        struct sockaddr_in client_addr;
        uint32_t len = sizeof(client_addr);
        int client = accept(sockfd, (struct sockaddr*)&client_addr, &len);
        printf("Conexão TLS recebida de %s\n", inet_ntoa(client_addr.sin_addr));

        SSL *ssl = SSL_new(ctx);
        SSL_set_fd(ssl, client);

        if (SSL_accept(ssl) <= 0) {
            ERR_print_errors_fp(stderr);
        } else {
            char buffer[MAX_MSG_SIZE] = {0};
            int bytes = SSL_read(ssl, buffer, sizeof(buffer) - 1);
            if (bytes > 0) {
                buffer[bytes] = '\0';
                printf("Mensagem recebida (hex):\n");
                for (int i = 0; i < bytes; i++) printf("%02X ", (unsigned char)buffer[i]);
                printf("\n");

                // Verifica se contém PID 123456789
                if (strstr(buffer, "123456789")) {
                    send_mllp_message(ssl, adt_response);
                } else {
                    printf("PID não reconhecido.\n");
                }
            }
        }

        SSL_shutdown(ssl);
        SSL_free(ssl);
        close(client);
    }

    close(sockfd);
    SSL_CTX_free(ctx);
    cleanup_openssl();
}
