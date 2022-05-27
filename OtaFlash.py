import os 
import sys
import http.server
import http.client
import ssl
import threading
import time

request_finished_event = threading.Event()
server_started_event = threading.Event()

class OneTimeHttpRequestHandler(http.server.SimpleHTTPRequestHandler):
    def do_GET(self):
        super().do_GET()
        print('Get request finished')
        request_finished_event.set()

def start_https_server(ota_image_dir, server_ip, server_port, server_file=None, key_file=None):
    os.chdir(ota_image_dir)
    global httpd
    #handler = http.server.SimpleHTTPRequestHandler
    handler = OneTimeHttpRequestHandler
    httpd = http.server.HTTPServer((server_ip, server_port), handler)

    httpd.socket = ssl.wrap_socket(httpd.socket,
                                   keyfile=key_file,
                                   certfile=server_file, server_side=True)
    server_started_event.set()
    httpd.serve_forever()

def start_binary_server():
    this_dir = os.path.dirname(os.path.realpath(__file__))
    bin_dir = os.path.join(this_dir, 'build')
    port = 80
    cert_dir = os.path.join(this_dir, 'components', 'web_flash', 'certificates')
    print('Starting HTTPS server at "https://:{}"'.format(port))
    start_https_server(bin_dir, '', port,
                       server_file=os.path.join(cert_dir, 'otacacert.pem'),
                       key_file=os.path.join(cert_dir, 'otaprvtkey.pem'))

def initiate_update(ip):
    ssl_context = ssl.SSLContext(ssl.PROTOCOL_TLSv1_2)
    ssl_context.verify_mode = ssl.CERT_NONE
    ssl_context.check_hostname = False
    client = http.client.HTTPSConnection(ip, 443, context=ssl_context)
    client.request("POST", "/flash", "DataHub")


if __name__ == '__main__':     
    server_thread = threading.Thread(target=start_binary_server)
    server_thread.start()
    server_started_event.wait()
    print("Sending download request")
    initiate_update(sys.argv[1])
    request_finished_event.wait(30)
    time.sleep(0.3)
    httpd.shutdown()
    server_thread.join()
