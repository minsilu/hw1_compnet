
import socket
import argparse

def main():
    parser = argparse.ArgumentParser(description="UDP Client")
    parser.add_argument('-ip', type=str, default="127.0.0.1", help="The IP address of the FTP server.")
    parser.add_argument('-port', type=int, default=21, help="The port number of the FTP server.")
    args = parser.parse_args()

    server_ip = args.ip
    server_port = args.port
    size = 8192

    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        for i in range(51):  # Send messages numbered from 0 to 50
            msg = str(i)
            sock.sendto(msg.encode(), (server_ip, server_port))
            response, _ = sock.recvfrom(size)
            print(response.decode())
        sock.close()
    except Exception as e:
        print(f"Cannot reach the server: {e}")


if __name__ == "__main__":
    main()
