import socket
import os
import sys
import argparse



class FTPClient:
    def __init__(self, host, port=21):
        self.host = host
        self.port = port
        self.sock = None
        self.data_sock = None
        self.port_sock = None
        self.transfer_type = None
        self.write_type = 'wb'
        #self.is_passive = False
        
   
    def log(self, message):
        # TODO: This method can be modified to log messages to a GUI or a file
        print(message)

    def connect(self):
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.connect((self.host, self.port))
            self.log(f"Connected to {self.host}:{self.port}")
            return self.read_response()
        except socket.error as e:
            self.log(f"Error: Failed to connect to {self.host}:{self.port} - {e}")
            return None

    def send_command(self, command):
        self.sock.send(f"{command}\r\n".encode())
        return self.read_response()

    def send_command_with_params(self, verb, params):
        if verb == 'REST':
            self.write_type = 'ab'
        self.send_command(f"{verb} {params}")

    def read_response(self):
        response_lines = []
        while True:
            response_chunk = self.sock.recv(8192).decode()
            response_lines.append(response_chunk.strip())
            # Check if the last line ends with a terminating code
            if response_chunk.endswith("\r\n"):
                break
        full_response = ''.join(response_lines)
        self.log(full_response.strip())
        # TODO: check if you need to handle the '-' connect situation
        return full_response

    def login(self, username, password):
        self.send_command(f"USER {username}")
        self.send_command(f"PASS {password}")

    def passive_mode(self):
        response = self.send_command("PASV")
        try:
            if response.startswith('227'):
                import re
                numbers = re.findall(r'\d+', response)
                port = (int(numbers[-2]) << 8) + int(numbers[-1])
                self.data_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                self.data_sock.connect((self.host, port))
                self.transfer_type = 'PASV'
            else:
                self.log(f"Error: Failed to establish passive mode - {response}")
        except Exception as e:
            self.log(f"Error: Failed to establish passive mode - {e}")

    def active_mode(self, local_ip, local_port):
        try:
            self.port_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.port_sock.bind((local_ip, local_port)) 
            self.port_sock.listen(1) 
            ip_parts = local_ip.split('.')
            port_hi = local_port // 256
            port_lo = local_port % 256
            self.send_command(f"PORT {','.join(ip_parts)},{port_hi},{port_lo}")
            self.transfer_type = 'PORT'
        except Exception as e:
            self.log(f"Error: Failed to establish active mode - {e}")

    def retrieve_file(self, filename):
        if not self.is_passive:
            self.passive_mode()
        self.send_command(f"RETR {filename}")
        with open(filename, 'wb') as file:
            while True:
                data = self.data_sock.recv(8192)
                if not data:
                    break
                file.write(data)
        self.data_sock.close()
        self.read_response()

    def store_file(self, filename):
        if not self.is_passive:
            self.passive_mode()
        self.send_command(f"STOR {filename}")
        with open(filename, 'rb') as file:
            while True:
                data = file.read(8192)
                if not data:
                    break
                self.data_sock.send(data)
        self.data_sock.close()
        self.read_response()

    def quit(self):
        self.send_command("QUIT")
        if self.data_sock:
            self.data_sock.close()  # Close the passive socket if it exists
            self.data_sock = None 
        self.sock.close()
        
    def list_files(self):
        if not self.is_passive:
            self.passive_mode()
        self.send_command("LIST")
        data = self.data_sock.recv(8192).decode()
        print(data)
        self.data_sock.close()
        self.read_response()

    def remove_directory(self, dirname):
        self.send_command(f"RMD {dirname}")

def main():
    parser = argparse.ArgumentParser(description='FTP Client')
    parser.add_argument('-ip', type=str, default='127.0.0.1', help='Server IP address')
    parser.add_argument('-port', type=int, default=21, help='Server port')
    args = parser.parse_args()

    client = FTPClient(args.ip, args.port)
    client.connect()

    while True:
        command = input("ftp> ").strip()
        verb = command.split(' ', 1)[0]
        # params = input_data.split(" ", 1)[1]
        if command.lower() == 'quit':
            client.quit()
            break
        elif verb in ['USER','PASS','SYST','PWD']:
            client.send_command(command)
        elif verb in['MKD','CWD','RMD','DELE','REST','TYPE']:
            client.send_command_with_params(verb, command.split(' ', 1)[1])
            # TODO: if it is necessary to need a flag to change write mode to ab when type rest
        elif verb == 'PASV':
            client.passive_mode()
        elif verb == 'PORT':
            parts = command.split(' ', 1)[1].split(',')
            ip = '.'.join(parts[:4])
            port = (int(parts[4]) << 8) + int(parts[5])
            client.active_mode(ip, port)
        elif verb == 'RETR':
            client.retrieve_file(command.split(' ', 1)[1])
        elif verb == 'STOR':
            client.store_file(command.split(' ', 1)[1])
        elif verb == 'LIST':
            client.list_files()
        elif verb == 'APPE':
            break
        else:
            client.log(f"Error: Invalid command: {command}")

if __name__ == "__main__":
    main()