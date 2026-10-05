from flask import Flask
from socket import *
import sys

app = Flask(__name__)

@app.route('/')
def hello_world():  
    #count = 0  
    #for i in range(0, 5000000):
    #    count += i 
        
    return "Hello WOrld"
    #return "Welcome Flask appllication!" * 3000
        

if __name__ == '__main__':
    
    if len(sys.argv) != 2:
        print("[Usage] python3 api_server.py [API Server Port]")
        exit(1)

    # Flask run 
    app.run(host='0.0.0.0', port=int(sys.argv[1]))
