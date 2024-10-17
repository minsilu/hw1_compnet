
import socket
import argparse

def main():
    parser = argparse.ArgumentParser(description="UDP Server")
    parser.add_argument('-port', type=int, default=21, help="The TCP port number to bind the server to.")
    parser.add_argument('-root', type=str, default="/tmp", help="The pathname of the directory tree as the root for all requests.")
    args = parser.parse_args()

    server_port = args.port
    size = 8192

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(('', server_port))
    sequence_number = 0  # Initialize the sequence number

    try:
        print(f"Server running on port {server_port}...")
        while True:
            data, address = sock.recvfrom(size)
            sequence_number += 1
            response = f"{sequence_number} {data.decode()}"
            sock.sendto(response.encode(), address)
            
    except Exception as e:
        print(f"Server error: {e}")
    finally:
        sock.close()

if __name__ == "__main__":
    main()
