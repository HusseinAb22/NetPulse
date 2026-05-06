import socket
import threading
import sys

def listen_for_messages(sock):
    """This function runs on a background thread and constantly reads from the server"""
    while True:
        try:
            data = sock.recv(1024)
            if not data:
                print("\nServer closed the connection.")
                return

            # Print the incoming message, then reprint the input prompt
            # so the terminal doesn't look messy
            print(f"\nServer: {data.decode()}")
            print("> ", end="", flush=True)
        except:
            print("\nDisconnected.")
            return

def main():
    client_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    try:
        print("Attempting to connect to localhost:9000...")
        client_sock.connect(('127.0.0.1', 9000))
        print("Connected successfully! Type your message (or 'quit' / 'q' to exit).")
    except Exception as e:
        print(f"Failed to connect: {e}")
        return

    # Spin up the background listening thread!
    listener = threading.Thread(target=listen_for_messages, args=(client_sock,), daemon=True)
    listener.start()

    # Main thread handles writing
    while True:
        try:
            msg = input("> ")
            if msg.lower() in ['quit', 'q']:
                print("Closing the connection.")
                break
            if msg.strip():
                client_sock.send((msg+'\n').encode())
        except KeyboardInterrupt:
            break

    client_sock.close()

if __name__ == "__main__":
    main()