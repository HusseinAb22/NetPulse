import socket

def start_client():
    # 1. Create a TCP socket (AF_INET = IPv4, SOCK_STREAM = TCP)
    client_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    try:
        # 2. Connect to the C++ server
        print("Attempting to connect to localhost:9000...")
        client_sock.connect(('127.0.0.1', 9000))
        print("Connected successfully! Type your message (or 'quit' / 'q' to exit).")

        # 3. The Interactive Echo Loop
        while True:
            # Get input
            message = input("> ")

            if message.lower() == 'quit' or message.lower() == 'q':
                break

            # We must encode the string into utf-8 bytes before sending.
            client_sock.sendall(message.encode('utf-8'))

            # Wait for the C++ server to echo the bytes back
            response_bytes = client_sock.recv(1024)

            # Decode the raw bytes back into a readable string
            print(f"Server Echo: {response_bytes.decode('utf-8')}")

    except ConnectionRefusedError:
        print("\nError: Connection refused. Is your C++ server currently running?")
    except KeyboardInterrupt:
        print("\nClient interrupted by user.")
    finally:
        # 4. Clean up
        print("Closing the connection.")
        client_sock.close()

if __name__ == "__main__":
    start_client()