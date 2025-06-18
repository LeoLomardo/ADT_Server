#!/usr/bin/env python3
import socket
import ssl

IP = '0.0.0.0'
PORT = 3502
HL7_FILE = 'mensagem.hl7'

def load_hl7_message(path):
    with open(path, 'r') as f:
        msg = f.read().replace('\n', '\r')
    return f'\x0b{msg}\x1c\r'.encode('utf-8')  # MLLP envelope

context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
context.load_cert_chain(certfile='cert.pem', keyfile='key.pem')
context.set_ciphers('HIGH:!aNULL:!MD5')
context.options |= ssl.OP_NO_TLSv1 | ssl.OP_NO_TLSv1_1

print(f"[+] Servidor TLS HL7 escutando em {IP}:{PORT}")
with socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0) as sock:
    sock.bind((IP, PORT))
    sock.listen(5)
    with context.wrap_socket(sock, server_side=True) as ssock:
        while True:
            client_conn, addr = ssock.accept()
            print(f"[+] Conexão segura de {addr}")

            hl7_msg = load_hl7_message(HL7_FILE)
            client_conn.sendall(hl7_msg)
            print("[>] Mensagem HL7 enviada")

            try:
                ack = client_conn.recv(2048)
                print(f"[<] ACK recebido ({len(ack)} bytes):")
                print(ack.decode(errors='ignore'))
            except Exception as e:
                print("Erro ao receber ACK:", e)

            client_conn.close()
