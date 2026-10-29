
#matrix V+ --> 1, V- --> 2, I+ -->3, I- --> 4
#DMM[i] refers to DMM[i+1], GND ---> 9

import requests

class RTController:

    # The matrix configs (config_1..config_5 for measurement, config_1..config_8 for diagnostic),
    # the address encoding and all validation live in the firmware; the client only forwards
    # the method arguments.

    def __build_json(self, op_mode, mode, Quadrant, DB, Samtec, FportPairs):
        return {"Quadrant": Quadrant,
                "DB": DB,
                "Samtec": Samtec,
                "FportPairs": FportPairs,
                "enable": 1,
                "mode": op_mode,
                "config": mode,
                }

    def measure(self,mode= None, Quadrant = None, DB=0,Samtec=0,FportPairs=None):
        return self.__build_json("measurement", mode, Quadrant, DB, Samtec, FportPairs)

    def diagnostic(self,mode= None, Quadrant = None, DB=0,Samtec=0,FportPairs=None):
        return self.__build_json("diagnostic", mode, Quadrant, DB, Samtec, FportPairs)


ctrl = RTController()

import time
import json
from datetime import datetime

session = requests.Session()

print(ctrl.measure(mode="config_1",Quadrant=1, DB=1, Samtec = 1, FportPairs= "F5F6"))



count = 0   
while(True):
    #while(True):
    response = session.post("http://192.168.1.50:5000",
                            json=ctrl.measure(mode="config_1",Quadrant=1, DB=1, Samtec = 1, FportPairs= "F5F6"),
                            timeout =5)
    resp = response.json()
    print(resp)
    #time.sleep(1)
    if resp != None and resp['status'] != 'OK':
         break
  #ctrl.diagnostic(mode="config_1",DB=1, Samtec=1,FportPairs="F5F6")
    count = count +1
    status = resp.get("status") if isinstance(resp, dict) else None
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
    with open("response_log.txt", "a", buffering=1) as log_file:
        log_file.write(f"{timestamp}, count={count}, status={status}\n")
    print(f"count-->{count}" )
    print(f"response :: {resp}")
    if resp !=None and resp['status'] == 'WRONG COMMAND':
         break
