# Adafruit CC3000 Library Test Data Generator
# Created by Tony DiCola (tony@tonydicola.com)
# Released with the same license as the Adafruit CC3000 library (BSD)

# Create a simple server to listen by default on port 9000 (or on the port specified in
# the first command line parameter), accept any connections, and generate random data for
# testing purposes.  If a number + newline is received as input from the connected client
# then random data of that number characters will be sent back in response.  Must be 
# terminated by hitting ctrl-c to kill the process!

from socket import *
import random
import sys
import threading


SERVER_PORT = 9000
if len(sys.argv) > 1:
	SERVER_PORT = sys.argv[1]

# Create listening socket
server = socket(AF_INET, SOCK_STREAM)

# Ignore waiting for the socket to close if it's already open.  See the python socket
# doc for more info (very bottom of http://docs.python.org/2/library/socket.html).
server.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)

# Listen on any network interface for the specified port
server.bind(('', SERVER_PORT))
server.listen(5)

def generate_data(size):
	# Generate a string with size characters that are random values.
	return ''.join([chr(random.randrange(0, 255)) for i in range(size)])

# Worker process to take input and generate random output.
def process_connection(client, address):
	stream = client.makefile()
	# Read lines until the socket is closed.
	for line in stream:
		try: 
			# Try to interpret the input as a number, and generate that amount of
			# random test data to output.
			count = int(line.strip())
			print 'Sending', count, 'bytes of data to', address[0]
			stream.write(generate_data(count))
			stream.flush()
		except ValueError:
			# Couldn't parse the provided count. Ignore it and wait for more input.
			pass
	print 'Closing client connection from', address[0]
	client.close()

try:
	# Wait for connections and spawn worker threads to process them.
	print 'Waiting for new connections on port', SERVER_PORT
	while True:
		client, address = server.accept()
		print 'Client connected from', address[0]
		thread = threading.Thread(target=process_connection, args=(client, address))
		thread.setDaemon(True) # Don't block exiting if any threads are still running.
		thread.start()
except:
	server.close()