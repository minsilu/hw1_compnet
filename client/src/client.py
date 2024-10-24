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
            #self.log(f"Connected to {self.host}:{self.port}")
            return self.read_response()
        except socket.error as e:
            self.log(f"Error: Failed to connect to {self.host}:{self.port} - {e}")
            return "error"

    def send_command(self, command):
        self.sock.send(f"{command}\r\n".encode())
        return self.read_response()

    def send_command_with_params(self, verb, params):
        if verb == 'REST':
            self.write_type = 'ab'
        return self.send_command(f"{verb} {params}")

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
                return response
            else:
                self.log(f"Error: Failed to establish passive mode - {response}")
        except Exception as e:
            self.log(f"Error: Failed to establish passive mode - {e}")
            return "error"

    def active_mode(self, local_ip, local_port):
        try:
            self.port_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.port_sock.bind((local_ip, local_port)) 
            self.port_sock.listen(1) 
            ip_parts = local_ip.split('.')
            port_hi = local_port // 256
            port_lo = local_port % 256
            response = self.send_command(f"PORT {','.join(ip_parts)},{port_hi},{port_lo}")
            self.transfer_type = 'PORT'
            return response
        except Exception as e:
            self.log(f"Error: Failed to establish active mode - {e}")
            # return

    def retrieve_file(self, filename, local_path =''):
        if not filename:
            self.log("Error: Need remote file name")
            return "error"
        local_file = os.path.join(local_path, os.path.basename(filename))

        # Check the transfer mode and handle accordingly
        if self.transfer_type == 'PASV':
            self.send_command(f"RETR {filename}")
        elif self.transfer_type == 'PORT':
            self.send_command(f"RETR {filename}")
            # Accept the incoming connection for data transfer
            self.data_sock, addr = self.port_sock.accept()
            self.port_sock.close()  # Close the listening socket
            self.port_sock = None
        else:
            self.log("Error: PORT or PASV mode is needed")
            return "error"
        
        try:
            with open(local_file, self.write_type) as file:
                while True:
                    data = self.data_sock.recv(8192)
                    if not data:
                        break
                    file.write(data)
        except Exception as e:
            self.log(f"Error while writing to file: {e}")
            return "error"
        finally:
            # Close the data socket
            if self.data_sock:
                self.data_sock.close()
                self.data_sock = None

        response = self.read_response()
        self.write_type = 'wb'  
        # Check for success response
        if "226" not in response:
            os.remove(local_file)  
            return "error"

        self.transfer_type = None  
        return response
        
    def store_file(self, filename):
        if not filename:
            self.log("Error: Need local file name")
            return "error"
        try:
            with open(filename, 'rb') as f:
                pass  
        except Exception as e:
            self.log(f"Error: {e}")
            return "error"

        # Handle the transfer mode
        if self.transfer_type == 'PASV':
            # In PASV mode, the data connection is already established
            self.send_command(f"STOR {filename}")
        elif self.transfer_type == 'PORT':
            # In PORT mode, we need to accept the incoming connection
            self.send_command(f"STOR {filename}")
            self.data_sock, addr = self.port_sock.accept()
            self.port_sock.close()  # Close the listening socket
            self.port_sock = None
        else:
            self.log("Error: PORT or PASV mode is needed")
            return "error"

        # Upload the file
        try:
            with open(filename, 'rb') as file:
                while True:
                    data = file.read(8192)  
                    if not data:
                        break
                    self.data_sock.send(data)  
        except Exception as e:
            self.log(f"Error while sending file: {e}")
            return "error"
        finally:
            if self.data_sock:
                self.data_sock.close()
                self.data_sock = None

        response = self.read_response()
        self.transfer_type = None  

        if "226" not in response:
            self.log("Error: File transfer failed.")
            return "error"

        return response

    def list_files(self):
        self.list_result = ""

        if self.transfer_type == 'PASV':
            self.send_command("LIST")  
        elif self.transfer_type == 'PORT':
            self.send_command("LIST")  
            self.data_sock, addr = self.port_sock.accept()
            self.port_sock.close()  
            self.port_sock = None
        else:
            self.log("Error: PORT or PASV mode is needed")
            return "error"

        try:
            while True:
                data = self.data_sock.recv(8192)  
                if not data:
                    break  
                self.list_result += data.decode()  
                self.log(data.decode(), end='')  
        except Exception as e:
            self.log(f"Error while receiving file list: {e}")
            return "error"
        finally:
            if self.data_sock:
                self.data_sock.close()
                self.data_sock = None

        response = self.read_response()
        self.transfer_type = None  
        return response

    def remove_directory(self, dirname):
        self.send_command(f"RMD {dirname}")

    # def quit(self):
    #     self.send_command("QUIT")
    #     if self.data_sock:
    #         self.data_sock.close()  # Close the passive socket if it exists
    #         self.data_sock = None 
    #     self.sock.close()

def main():
    parser = argparse.ArgumentParser(description='FTP Client')
    parser.add_argument('-ip', type=str, default='127.0.0.1', help='Server IP address')
    parser.add_argument('-port', type=int, default=21, help='Server port')
    args = parser.parse_args()

    client = FTPClient(args.ip, args.port)
    client.connect()

    while True:
        command = input().strip() ## "ftp> "
        verb = command.split(' ', 1)[0]
        # params = input_data.split(" ", 1)[1]
        if verb in ['USER','PASS','SYST','PWD','QUIT']:
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
        elif verb.lower() == 'quit':
            break
        else:
            client.log(f"Error: Invalid command: {command}")

if __name__ == "__main__":
    main()