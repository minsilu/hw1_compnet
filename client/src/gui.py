import sys
from PyQt5.QtWidgets import QApplication, QMainWindow
from PyQt5 import uic

class FTPClientGUI(QMainWindow):
    def __init__(self):
        super().__init__()
        uic.loadUi('ftp_client.ui', self)
        
        # Connect buttons to functions
        self.connectButton.clicked.connect(self.connect_to_server)
        self.uploadButton.clicked.connect(self.upload_file)
        self.downloadButton.clicked.connect(self.download_file)
        
    def connect_to_server(self):
        # Implement connection logic
        pass
    
    def upload_file(self):
        # Implement file upload logic
        pass
    
    def download_file(self):
        # Implement file download logic
        pass

if __name__ == '__main__':
    app = QApplication(sys.argv)
    window = FTPClientGUI()
    window.show()
    sys.exit(app.exec_())