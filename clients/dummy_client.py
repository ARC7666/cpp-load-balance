import threading 
import requests
import time
import sys

result = {}

def dummy(ip, port, index):
    global result
    try:
        res = requests.get("http://{0}:{1}".format(ip, port), timeout=5)
        result[index] = 1
    except:
        print("[Error] disconnected from server: "+ str(index)+" thread")
        result[index] = 0
        return 
    
    print("[Log] Get Data from server: "+ str(index)+" thread")
    return


if len(sys.argv) != 4:
    print("[USAGE] python3 dummy_client.py [Server IP] [Server Port] [num of request]")
    exit(1)

task = []

start = time.time()

round = int(sys.argv[3])

for i in range(0, round):
    result[i] = 0


for i in range(0, round):
    r_thr = threading.Thread(target=dummy, args=(sys.argv[1], sys.argv[2], i, ))
    task.append(r_thr)
    r_thr.start() 


for t in task:
    t.join()
    
suc = 0
fail = 0
for i in range(0, round):
    if result[i] == 1:
        suc += 1
    else:
        fail += 1

per = (suc / round) * 100
print("성공률 : {0}%".format(per))
print("성공 횟수 : {0}".format(suc))
print("실패 횟수 : {0}".format(fail))
    
end = time.time()

print(f"FIN: {end - start:.5f} sec")
