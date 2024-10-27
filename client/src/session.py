import socket
import os
import random
from datetime import datetime
import time
import re
import fnmatch
import glob
from utils import *
import subprocess

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
        self.command_map = {
            'cd': 'CWD',
            'pwd': 'PWD',
            'rmdir': 'RMD',
            'delete': 'DELE',
            'mkdir': 'MKD',
        }
   
    def log(self, message,end="\n"):
        print(message,end=end)

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

    
    def read_response(self, timeout=5):
        # Set the socket timeout
        self.sock.settimeout(timeout)
        try:
            f = self.sock.makefile()
            line = f.readline().strip()
            if line:
                self.log(line)
            else:
                return None
            return line
        except socket.timeout:
            return None
        except Exception as e:
            return None
        finally:
            self.sock.settimeout(None)

    def login(self, username = 'anonymous', password = 'pass@youremail'):
        self.send_command(f"USER {username}")
        self.send_command(f"PASS {password}")

    def passive_mode(self):
        response = self.send_command("PASV")
        if self.data_sock:
            self.data_sock.close()
        try:
            if response.startswith('227'):
                numbers = re.findall(r'\d+', response)
                port = (int(numbers[-2]) << 8) + int(numbers[-1])
                self.data_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                self.data_sock.connect((self.host, port))
                self.transfer_type = 'PASV'
                return response
            else:
                self.log(f"{response}")
        except Exception as e:
            self.log(f"Error: Failed to establish passive mode - {e}")
            return "error"

    def active_mode(self, local_ip, local_port):
        if self.data_sock:
            self.data_sock.close()
        if self.port_sock:
            self.port_sock.close()
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

    def close_data_connection(self):
        """Closes the data socket if open."""
        if self.data_sock:
            try:
                self.data_sock.close()
            except Exception as e:
                self.log(f"Error closing data socket: {e}")
            self.data_sock = None
            
    def retrieve_file(self, remote_filename, local_path =''):
        if not remote_filename:
            self.log("Error: Need remote file name")
            return "error"
        local_file = os.path.join(local_path, os.path.basename(remote_filename))

        # Check the transfer mode and handle accordingly
        if self.transfer_type == 'PASV':
            response = self.send_command(f"RETR {remote_filename}")
        elif self.transfer_type == 'PORT':
            response = self.send_command(f"RETR {remote_filename}")
            self.data_sock, addr = self.port_sock.accept()
            self.port_sock.close()  # Close the listening socket
            self.port_sock = None
        else:
            self.log("Error: PORT or PASV mode is needed")
            return "error"
    
        if "550" in response:
            self.close_data_connection()
            self.transfer_type = None 
            self.write_type = 'wb'
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
            self.close_data_connection()
            self.transfer_type = None 

        # Read the response
        response += self.read_response()
        # Check for success response
        if "226" not in response:
            os.remove(local_file)  
            return "error"
        
        self.write_type = 'wb' 
        return response
          
    def store_file(self, filename):
        if not filename:
            self.log("Error: Need local file name")
            return "error"
        if not os.path.isfile(filename):
            self.log(f"Error: Local file {filename} does not exist")
            return "error"

        # Check if the file exists
        try:
            with open(filename, 'rb') as f:
                pass  
        except Exception as e:
            self.log(f"Error: {e}")
            return "error"

        # Handle the transfer mode
        if self.transfer_type == 'PASV':
            # In PASV mode, the data connection is already established
            response = self.send_command(f"STOR {filename}")
        elif self.transfer_type == 'PORT':
            # In PORT mode, we need to accept the incoming connection
            response = self.send_command(f"STOR {filename}")
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
            self.close_data_connection()
            self.transfer_type = None
            
        response += self.read_response()

        return response

    def append_file(self, filename, offset=0):
        if not filename:
            self.log("Error: Need local file name")
            return "error"
        if not os.path.isfile(filename):
            self.log(f"Error: Local file {filename} does not exist")
            return "error"

        # Check if the file exists and get its size
        try:
            file_size = os.path.getsize(filename)
            if offset > file_size:
                self.log("Error: Starting position exceeds file size.")
                return "error"
        except Exception as e:
            self.log(f"Error: {e}")
            return "error"

        # Handle the transfer mode
        if self.transfer_type == 'PASV':
            response = self.send_command(f"APPE {filename}")
        elif self.transfer_type == 'PORT':
            response = self.send_command(f"APPE {filename}")
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
                file.seek(offset)
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
            self.transfer_type = None

        response += self.read_response()

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

        file_info = ""
        try:
            while True:
                data = self.data_sock.recv(8192)  
                if not data:
                    break   
                self.log(data.decode(), end='')  
                file_info += data.decode()
        except Exception as e:
            self.log(f"Error while receiving file list: {e}")
            return "error"
        finally:
            if self.data_sock:
                self.data_sock.close()
                self.data_sock = None
        file_info += self.read_response()
        self.transfer_type = None  
        return file_info

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
        local_file = os.path.join(local_path, base_name)
        
        # Check if the local file exists
        if os.path.exists(local_file):
            response = self.ls_remote_files(remote_path)
            # Check if the remote file exists
            if "226" in response:
                local_size = os.path.getsize(local_file)
                local_mtime_str = time.strftime('%b %d %H:%M', time.localtime(os.path.getmtime(local_file)))
                
                remote_info_lines = response.splitlines()

                for line in remote_info_lines:
                    # Check if the line contains the target file name
                    if base_name in line:
                        parts = line.split()
                        remote_size = int(parts[4])  
                        remote_mtime_str = ' '.join(parts[5:8])  
                        local_mtime_dt = datetime.strptime(local_mtime_str, '%b %d %H:%M')
                        remote_mtime_dt = datetime.strptime(remote_mtime_str, '%b %d %H:%M')
                        break
                # Check conditions for resuming transfer
                if (local_size < remote_size) and (local_mtime_dt > remote_mtime_dt):
                    self.send_command_with_params("REST", local_size)  # Set the restart position
                    return self.get_remote_file(remote_path, local_path = local_path, write_type = self.write_type, set_rest = False)

        # If conditions are not met, perform a full transfer
        return self.get_remote_file(remote_path, local_path = local_path)
  
    def mget_remote_file(self, remote_files):
        if not remote_files:
            self.log("Error: No remote files specified.")
            return "error"

        remote_files = remote_files.strip().split()
        all_files = []
        for remote_file in remote_files:
            if "*" in remote_file:
                response = self.ls_remote_files('.')

                for line in response.splitlines():
                    if line and not re.match(r'^\d+', line) and not line.startswith("d"): 
                        parts = line.split()
                        filename = parts[-1]
                        if fnmatch.fnmatch(filename, remote_file): 
                            all_files.append(filename)
            else:
                all_files.append(remote_file)
                
        for remote_file in all_files:
            response = self.get_remote_file(remote_file)
            if response == "error":
                self.log(f"Failed to download '{remote_file}'.")
                
    def put_local_file(self, local_path, append = False, offset = 0):
        if not local_path:
            self.log("Error: Need local file name")
            return "error"
        if not os.path.isfile(local_path):
            self.log(f"Error: Local file {local_path} does not exist")
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
        
        if append:
            self.append_file(local_path, offset)
        else:
            self.store_file(local_path)

    def reput_local_file(self, local_path):
        if not local_path:
            self.log("Error: Need local file name")
            return "error"
        if not os.path.isfile(local_path):
            self.log(f"Error: Local file {local_path} does not exist")
            return "error"
        
        base_name = os.path.basename(os.path.basename(local_path))
        # Check if the remote file exists
        response = self.ls_remote_files(base_name)
        if "226" in response:
            local_size = os.path.getsize(local_path)
            local_mtime_str = time.strftime('%b %d %H:%M', time.localtime(os.path.getmtime(local_path)))

            remote_info_lines = response.splitlines()

            for line in remote_info_lines:
                # Check if the line contains the target file name
                if base_name in line:
                    parts = line.split()
                    remote_size = int(parts[4])  
                    remote_mtime_str = ' '.join(parts[5:8])  
                    local_mtime_dt = datetime.strptime(local_mtime_str, '%b %d %H:%M')
                    remote_mtime_dt = datetime.strptime(remote_mtime_str, '%b %d %H:%M')
                    break
            # Check conditions for resuming transfer
            if remote_size < local_size and remote_mtime_dt > local_mtime_dt:
                return self.put_local_file(local_path, append = True, offset = remote_size)

        # If conditions are not met, perform a full transfer
        return self.put_local_file(local_path)

    def mput_local_file(self, local_files):
        if not local_files:
            self.log("Error: No local files specified.")
            return "error"

        local_files = local_files.strip().split()
        all_files = []
        for local_file in local_files:
            if "*" in local_file:
                all_files.extend(glob.glob(local_file))
            else:
                all_files.append(local_file)

        for local_file in all_files:
            # Prompt for confirmation
            response = self.put_local_file(local_file)
            if response == "error":
                self.log(f"Failed to upload '{local_file}'.")

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

    def open_connection(self, host, port):
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
            path = self.local_path  
        try:
            result = subprocess.run(['ls', '-l', path], capture_output=True, text=True, check=True)
            self.log(result.stdout, end="")  
        except subprocess.CalledProcessError as e:
            self.log(f"Error ls local files: {e.stderr}")

    def show_help(self, command=None):
        commands_info = {
            'login': 'to login to the server',
            'cd': 'to change the current directory on the server',
            'pwd': 'to find out the current directory on the server',
            'rmdir': 'to remove a directory on the server',
            'mkdir': 'to create a directory on the server',
            'delete': 'to delete a file on the server',
            'quit': 'to exit the FTP environment',
            'bye': 'to exit the FTP environment',
            'close': 'to terminate a session with the server',
            'open': 'to open a new FTP session',
            
            'get': 'to download a file from the server',
            'reget': 'to resume downloading a file from the server',
            'mget': 'to download multiple files from the server',
            'put': 'to upload a file to the server',
            'reput': 'to resume uploading a file to the server',
            'mput': 'to upload multiple files to the server',
            'ls': 'to list files in a specified directory on the server',
            
            'lcd': 'to change the current directory on the local machine',
            'lpwd': 'to print the current working directory on the local machine',
            'lls': 'to list files in a specified directory on the local machine',
            'help': 'to request a list of all available FTP commands',
            
            'USER': 'Send the username to the server for authentication',
            'PASS': 'Send the password to the server for authentication',
            'SYST': 'Request the system type of the server',
            'PWD': 'Print the current working directory on the server',
            'QUIT': 'Terminate the FTP connection',
            'MKD': 'Create a directory on the server',
            'CWD': 'Change the current directory on the server',
            'RMD': 'Remove a directory on the server',
            'DELE': 'Delete a file on the server',
            'REST': 'Resume a file transfer from a specified position',
            'TYPE': 'Set the transfer type (ASCII or binary)',
            'PASV': 'Enter passive mode for data transfer',
            'PORT': 'Enter active mode for data transfer',
            'RETR': 'Download a file from the server',
            'STOR': 'Upload a file to the server',
            'LIST': 'List files in the current directory on the server',
            'APPE': 'Append data to a file on the server',
        }

        if command:
            if command in commands_info:
                self.log(f"{command}: {commands_info[command]}")
            else:
                self.log(f"Error: No information available for command '{command}'.")
        else:
            for cmd in sorted(commands_info.keys()): 
                self.log(f"    {cmd}",end="")
            self.log('',end="\n")
