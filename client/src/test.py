import os

class FileRetriever:
    def __init__(self):
        pass

    def log(self, message):
        print(message)

    def retrieve_file(self, remote_filename, local_path=''):
        if not remote_filename:
            self.log("Error: Need remote file name")
            return "error"
        
        local_file = os.path.join(local_path, os.path.basename(remote_filename))
        return local_file

# Test Module
def test_retrieve_file():
    # Create an instance of FileRetriever
    retriever = FileRetriever()

    # Get user input for remote_filename and local_path
    remote_filename = input("Enter the remote filename (with path if necessary): ").strip()
    local_path = input("Enter the local path (leave empty for current directory): ").strip()

    # Call retrieve_file method and print the result
    result = retriever.retrieve_file(remote_filename, local_path)
    print("Local file path:", result)

# Run the test
if __name__ == "__main__":
    test_retrieve_file()
