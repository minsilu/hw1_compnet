import socket
import os
import sys
import argparse
import random


class FTPClient:
    def __init__(self, host, port=21):
        self.host = host
        self.port = port
        self.sock = None
        self.data_sock = None
        self.port_sock = None
        self.transfer_type = None
        self.write_type = 'wb'
        self.local_path = os.getcwd() 
        #self.is_passive = False
        self.command_map = {
            'cd': 'CWD',
            'pwd': 'PWD',
            'rmdir': 'RMD',
            'delete': 'DELE',
            'mkdir': 'MKD',
        }
   
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
        f = self.sock.makefile()
        line = f.readline().strip() 
        if line[3] == '-':
            code = line[:3]
            while True:
                next_line = f.readline().strip()
                line = line + next_line
                if next_line[3] != '-' or next_line[:3] != code:
                    break
        self.log(line)
        return line
        
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

    def retrieve_file(self, remote_filename, local_path =''):
        if not remote_filename:
            self.log("Error: Need remote file name")
            return "error"
        local_file = os.path.join(local_path, os.path.basename(remote_filename))

        # Check the transfer mode and handle accordingly
        if self.transfer_type == 'PASV':
            self.send_command(f"RETR {remote_filename}")
        elif self.transfer_type == 'PORT':
            self.send_command(f"RETR {remote_filename}")
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
            self.transfer_type = None 
        
        response = self.read_response()
        
        # Check for success response
        if "226" not in response:
            os.remove(local_file)  
            return "error"
        
        self.write_type = 'wb' # only when transfer successful the rest will be overwritten
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
            self.transfer_type = None

        response = self.read_response()
          
        if "226" not in response:
            self.log("Error: File transfer failed.")
            return "error"

        return response

    def append_file(self, filename, a=0):
        if not filename:
            self.log("Error: Need local file name")
            return "error"

        # Check if the file exists and get its size
        try:
            file_size = os.path.getsize(filename)
            if a > file_size:
                self.log("Error: Starting position exceeds file size.")
                return "error"
        except Exception as e:
            self.log(f"Error: {e}")
            return "error"

        # Handle the transfer mode
        if self.transfer_type == 'PASV':
            self.send_command(f"APPE {filename}")
        elif self.transfer_type == 'PORT':
            self.send_command(f"APPE {filename}")
            self.data_sock, addr = self.port_sock.accept()
            self.port_sock.close()  
            self.port_sock = None
        else:
            self.log("Error: PORT or PASV mode is needed")
            return "error"

        # Upload the file starting from the specified position
        try:
            with open(filename, 'rb') as file:
                # Move to the specified starting position
                file.seek(a)
                while True:
                    data = file.read(8192)  
                    if not data:
                        break
                    self.data_sock.send(data)  # Send data to the server
        except Exception as e:
            self.log(f"Error while sending file: {e}")
            return "error"
        finally:
            if self.data_sock:
                self.data_sock.close()
                self.data_sock = None

        response = self.read_response()
        self.transfer_type = None  # Reset transfer type

        if "226" not in response:
            self.log("Error: File append failed.")
            return "error"

        return response
    
    def list_files(self, path = None):
        if path is None:
            path = ''
        if self.transfer_type == 'PASV':
            self.send_command(f"LIST {path}")  
        elif self.transfer_type == 'PORT':
            self.send_command(f"LIST {path}")  
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
                self.log(data.decode())  
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

    def ls_remote_files(self, remote_path = None):
        if self.transfer_type == 'PORT':
            while True:
                port = random.randint(20001, 65535)
                try:
                    # Create a temporary socket to check if the port is available
                    temp_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                    temp_sock.bind((self.sock.getsockname()[0], port))  
                    temp_sock.close() 
                    break  # Exit the loop if the port is available
                except OSError:
                    # If the port is not available, try again
                    continue
                    
            ip = self.sock.getsockname()[0]
            self.active_mode(ip, port)
        else:
            self.passive_mode()    
        return self.list_files(remote_path)  

    def get_remote_file(self, remote_path, local_path=None, write_type='wb', set_rest = True):
        if not remote_path:
            self.log("Error: Need remote file name")
            return "error"
        
        if local_path is None:
            local_path = self.local_path 
        
        if self.transfer_type == 'PORT':
            while True:
                port = random.randint(20001, 65535)
                try:
                    # Create a temporary socket to check if the port is available
                    temp_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                    temp_sock.bind((self.sock.getsockname()[0], port))  
                    temp_sock.close() 
                    break  # Exit the loop if the port is available
                except OSError:
                    # If the port is not available, try again
                    continue
            ip = self.sock.getsockname()[0]
            self.active_mode(ip, port)
        else:
            self.passive_mode()
            
        if set_rest:
            self.send_command_with_params("REST", 0)
        self.write_type = write_type
        
        return self.retrieve_file(remote_path, local_path)   

    def reget_remote_file(self, remote_path, local_path=None):
        if not remote_path:
            self.log("Error: Need remote file name")
            return "error"

        if local_path is None:
            local_path = self.local_path 
        base_name = os.path.basename(remote_path)

        file_path = os.path.join(local_path, base_name)
        # Check if the local file exists
        if os.path.exists(file_path):
            local_size = os.path.getsize(file_path)
            local_mtime = os.path.getmtime(file_path)

            # Get remote file size and modification time
            response = self.ls_remote_files(remote_path)
            remote_info_lines = response.splitlines()
            
            remote_size = None
            remote_mtime = None
            for line in remote_info_lines:
                # Check if the line contains the target file name
                if base_name in line:
                    parts = line.split()
                    remote_size = int(parts[4])  
                    remote_mtime = ' '.join(parts[5:8])  
                    break
            
            # Check conditions for resuming transfer
            if (remote_size is not None) and (local_size < remote_size) and (local_mtime < remote_mtime):
                self.send_command_with_params("REST", local_size)  # Set the restart position
                return self.get_remote_file(remote_path, local_path = local_path, write_type = self.write_type, set_rest = False)

        # If conditions are not met, perform a full transfer
        return self.get_remote_file(remote_path, local_path = local_path)

    def put_local_file(self, local_path):
        if not local_path:
            self.log("Error: Need local file name")
            return "error"

        if self.transfer_type == 'PORT':
            while True:
                port = random.randint(20001, 65535)
                try:
                    # Create a temporary socket to check if the port is available
                    temp_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                    temp_sock.bind((self.sock.getsockname()[0], port))  
                    temp_sock.close() 
                    break  # Exit the loop if the port is available
                except OSError:
                    # If the port is not available, try again
                    continue
            ip = self.sock.getsockname()[0]
            self.active_mode(ip, port)
        else:
            self.passive_mode()

        return self.store_file(local_path)

    def reput_local_file(self, local_path):
        if not local_path:
            self.log("Error: Need local file name")
            return "error"

        # Check if the remote file exists
        response = self.send_command(f"LIST {local_path}")
        if response.startswith('550'):
            # File does not exist, perform a full transfer
            return self.put_local_file(local_path)

        # Get remote file size and modification time
        remote_info = response.splitlines()[-1]  # Get the last line for the file info
        remote_size = int(remote_info.split()[4])  # Assuming size is in the 5th column
        remote_mtime = remote_info.split()[5]  # Modify this based on your LIST output format

        local_size = os.path.getsize(local_path)
        local_mtime = os.path.getmtime(local_path)

        # Check conditions for resuming transfer
        if remote_size < local_size and remote_mtime < local_mtime:
            self.send_command(f"APPE {local_path}")  # Append to the existing file
            return self.store_file(local_path)

        # If conditions are not met, perform a full transfer
        return self.put_local_file(local_path)

    def close_connection(self):
        self.send_command("QUIT")
        if self.sock:
            self.sock.close()
        if self.data_sock:
            self.data_sock.close()
        if self.port_sock:
            self.port_sock.close()
        self.ip = None
        self.port = None
        self.sock = None
        self.data_sock = None
        self.port_sock = None
        self.transfer_type = None
        self.write_type = 'wb'
        self.local_path = os.getcwd()
        self.log("Connection closed.")

    def open_connection(self, host="127.0.0.1", port=21):
        self.host = host
        self.port = port
        self.connect()

    def change_local_directory(self, path):
        try:
            os.chdir(path)
            self.local_path = os.getcwd()
            self.log(f"Changed local directory to: {self.local_path}")
        except Exception as e:
            self.log(f"Error changing local directory: {e}")
    
    def show_local_path(self):
        self.log(self.local_path)
        
    def ls_local_files(self, path=None):
        if path is None:
            list_of_files = list(os.listdir(self.local_path))
        else:
            try:
                list_of_files = list(self.log(os.listdir(path)))
            except Exception as e:  
                self.log(f"Error ls local files: {e}")
        self.log(' '.join(list_of_files))

def check_sencond_parameter(parameter):
    if len(parameter.split(' ', 1))>1:
        return parameter.split(' ', 1)[1]
    else:
        return None
   
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
        if verb in client.command_map:
            verb = client.command_map[verb]  # cd, pwd, rmdir, delete, mkdir
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
            client.append_file(command.split(' ', 1)[1])
        elif verb.lower() == 'quit' or verb == 'bye':
            break
        elif verb == 'close':   
            client.close_connection()
        elif verb == 'open':
            client.open_connection(command.split(' ', 1)[1].split(' ')[0], int(command.split(' ', 1)[1].split(' ')[1]))
        elif verb == 'lcd':
            client.change_local_directory(command.split(' ', 1)[1])
        elif verb == 'lpwd':
            client.show_local_path()
        elif verb == 'lls':
            client.ls_local_files()
        elif verb == 'ls':
            client.ls_remote_files(check_sencond_parameter(command))
        elif verb ==  'get':
            client.get_remote_file(command.split(' ', 1)[1].split(' ')[0], local_path = check_sencond_parameter(command.split(' ', 1)[1]))
        elif verb == 'put':
            client.put_local_file(command.split(' ', 1)[1])
        elif verb == 'reget':
            client.reget_remote_file(command.split(' ', 1)[1].split(' ')[0], local_path = check_sencond_parameter(command.split(' ', 1)[1]))
        elif verb == 'reput':
            client.reput_local_file(command.split(' ', 1)[1])
        elif verb == 'mget':
            break
        elif verb == 'mput':
            break
        elif verb == 'help':
            break
    
        
        else:
            client.log(f"Error: Invalid command: {command}")

if __name__ == "__main__":
    main()