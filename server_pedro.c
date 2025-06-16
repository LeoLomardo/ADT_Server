#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SB 0x0B   // MLLP start block
#define EB 0x1C   // MLLP end block
#define CR 0x0D   // carriage return

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <monitor_ip> <port>\n", argv[0]);
        return 1;
    }

    const char *ip = argv[1];
    int port = atoi(argv[2]);

    // A minimal HL7 v2.3.1 ORU^R01 PDS message (numeric sample)
    const char *hl7 =
        "MSH|^~\\&|N15|BED|PDS|HOSP|202506161200||ORU^R01|0001|P|2.3.1\r\n"
        "PID|1||TEST123^^^N15^MR||Sample^Patient||19800101|M|||123 Any St.^^City^ST^12345||(123)456-7890|||||123-45-6789\r\n"
        "OBR|1||1001^PDS||42749-3^Heart rate^LN\r\n"
        "OBX|1|NM|42749-3^Heart rate^LN||75|bpm|60-100|N|||F|||202506161200\r";

    // Build MLLP frame
    size_t envelope_len = 1 + strlen(hl7) + 2;
    char *envelope = malloc(envelope_len + 1);
    if (!envelope) {
        perror("malloc");
        return 1;
    }
    envelope[0] = SB;
    memcpy(envelope+1, hl7, strlen(hl7));
    envelope[1 + strlen(hl7)] = EB;
    envelope[1 + strlen(hl7) + 1] = CR;
    envelope[envelope_len] = '\0';

    // Create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        free(envelope);
        return 1;
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port)
    };
    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid address: %s\n", ip);
        close(sock);
        free(envelope);
        return 1;
    }

    // Connect
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sock);
        free(envelope);
        return 1;
    }

    // Send message
    if (write(sock, envelope, envelope_len) != envelope_len) {
        perror("write");
        close(sock);
        free(envelope);
        return 1;
    }
    printf("Sent HL7 PDS message (%zu bytes)\n", envelope_len);

    // Read ACK
    char buf[4096];
    ssize_t n = read(sock, buf, sizeof(buf)-1);
    if (n > 0) {
        buf[n] = '\0';
        printf("Received ACK:\n%s\n", buf);
    } else {
        perror("read");
    }

    close(sock);
    free(envelope);
    return 0;
}