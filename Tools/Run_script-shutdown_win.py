import socket
import os
import sys
from getmac import get_mac_address

# --- CONFIGURAÇÕES ---
# Porta UDP para escutar. Deve ser a mesma configurada no ESP32 (config.udp_port)
UDP_PORT = 9

# Prefixo que define o pacote de shutdown (6 bytes de 0xEE)
SHUTDOWN_PREFIX = b'\xee\xee\xee\xee\xee\xee'

# Tamanho esperado do pacote (6 de prefixo + 16 * 6 de MAC = 102 bytes)
PACKET_SIZE = 102

def main():
    """
    Função principal que escuta por um pacote UDP especial, valida o MAC
    e desliga o sistema.
    """
    # 1. OBTER O ENDEREÇO MAC LOCAL
    try:
        my_mac_str = get_mac_address()
        if not my_mac_str:
            raise ValueError("Não foi possível obter o endereço MAC.")
        
        # Converte o MAC de string (ex: "00:1A:2B...") para bytes (ex: b'\x00\x1a\x2b...')
        # para facilitar a comparação. Removemos ':' e '-' antes da conversão.
        my_mac_bytes = bytes.fromhex(my_mac_str.replace(':', '').replace('-', ''))
        
        print(f"[*] MAC local detectado: {my_mac_str}")
        print(f"[*] A aguardar pacotes de shutdown destinados a este MAC...")

    except Exception as e:
        print(f"[!] Erro ao obter o MAC local: {e}")
        sys.exit(1)

    # Define o comando de shutdown para o sistema operativo atual
    is_windows = os.name == 'nt'
    shutdown_command = "shutdown /s /t 60" if is_windows else "sudo /sbin/shutdown -h now"

    # Cria e configura o socket UDP
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.bind(('0.0.0.0', UDP_PORT))
        print(f"[*] Listener iniciado na porta UDP {UDP_PORT}.")
    except OSError as e:
        print(f"[!] Erro ao iniciar o listener: {e}")
        print("[!] Verifique se a porta já está em uso ou se tem permissões.")
        sys.exit(1)

    while True:
        try:
            data, addr = sock.recvfrom(128)
            
            # 2. VALIDAR O PACOTE RECEBIDO
            # Extrai o MAC do pacote (os 6 bytes a seguir ao prefixo)
            packet_mac_bytes = data[6:12]

            # A condição agora verifica o tamanho, o prefixo E o endereço MAC
            if (len(data) == PACKET_SIZE and 
                data.startswith(SHUTDOWN_PREFIX) and 
                packet_mac_bytes == my_mac_bytes):
                
                print(f"\n[*] Pacote de shutdown VÁLIDO recebido de {addr[0]}")
                print(f"    -> MAC do pacote corresponde ao MAC local.")
                print(f"[*] A executar o comando: {shutdown_command}")
                
                # 3. EXECUTAR O SHUTDOWN
                os.system(shutdown_command)
                break
            # Opcional: Log de pacotes inválidos ou para outros MACs
            # else:
            #     print(f". (Pacote recebido de {addr[0]} ignorado: não corresponde aos critérios)")

        except KeyboardInterrupt:
            print("\n[*] Listener encerrado pelo utilizador.")
            break
        except Exception as e:
            print(f"[!] Ocorreu um erro: {e}")

    sock.close()

if __name__ == '__main__':
    main()