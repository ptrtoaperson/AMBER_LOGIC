
#matrix V+ --> 1, V- --> 2, I+ -->3, I- --> 4
#DMM[i] refers to DMM[i+1], GND ---> 9

import requests

class RTController:

    def __init__(self,url):
        self.url = url
        self.session = requests.Session()
     
        self.measurement_configs ={ "config_1"    :   {
            "1" :    3, #V+ ->DMM[2]
            "2" :    5, #V- ->DMM[4]
            "3" :    1, #I+ ->DMM[0]
            "4" :    7  #I- ->DMM[6]
        },
        "config_2"   :   {
            "1" :   3, #V+ ->DMM[2]
            "2" :   9, #V- ->GND
            "3" :   1  #I+ ->DMM[0]
        },
        "config_3"    :   {
            "1" :   7, #V+ ->DMM[6]
            "2" :   9, #V- ->GND
            "3" :   5 #I+ ->DMM[0] 
        },
        "config_4"  :   {
            "1" :   2, #V+ ->DMM[1]
            "2" :   3, #V- ->DMM[2]
            "3" :   1, #I+ ->DMM[0]
            "4" :   4  #I- ->DMM[3]
        },
        "config_5"  :   {
            "1" :   6, #V+ ->DMM[5]
            "2" :   7, #V- ->DMM[6]
            "3" :   5, #I+ ->DMM[4]
            "4" :   8  #I- ->DMM[7]
        } 
        }

        self.diagnostic_configs = { 
        "config_1" :    {
            "1" :   1, #V+ ->DMM[0]
            "2" :   2, #V- ->DMM[1]
            "3" :   1, #I+ ->DMM[0]
            "4" :   2  #I- ->DMM[1]
        },
        "config_2"  :   {
            "1" :   3, #V+ ->DMM[2]
            "2" :   4, #V- ->DMM[3]
            "3" :   3, #I+ ->DMM[2]
            "4" :   4  #I- ->DMM[3]
        },
        "config_3"  :   {
            "1" :   5, #V+ ->DMM[4]
            "2" :   6, #V- ->DMM[5]
            "3" :   5, #I+ ->DMM[4]
            "4" :   6  #I- ->DMM[5]
        },
        "config_4"  :   {
            "1" :   7, #V+ ->DMM[6]
            "2" :   8, #V- ->DMM[7]
            "3" :   7, #I+ ->DMM[6]
            "4" :   8  #I- ->DMM[7]
        },
        "config_5"  :   {
            "1" :   2, #V+ ->DMM[1]
            "2" :   3, #V- ->DMM[2]
            "3" :   1, #I+ ->DMMM[0]
            "4" :   4  #I- ->DMM[3]
        },
        "config_6"  :   {
            "1" :   6, #V+ ->DMM[5]
            "2" :   7, #V- ->DMM[6]
            "3" :   5, #I+ ->DMM[4]
            "4" :   8  #I- ->DMM[7]
        },
        "config_7"  :   {
            "1" :  3, #V+ ->DMM[2]
            "2" :  5, #V- ->DMM[4]
            "3" :  1, #I+ ->DMM[0]
            "4" :  7  #I- ->DMM[6]
        },
        "config_8"  :   {
            "1" :   1, #V+ ->DMM[0]
            "2" :   5, #V- ->DMM[4]
            "3" :   1, #I+ ->DMM[0]
            "4" :   5  #I- ->DMM[4]
        }
        }

    def __validate_FportsPairs(self, _FportPairs):
        F1F2  = 0b0
        F3F4 = 0b01
        F5F6 = 0b10
        F7F8 = 0b11
        if _FportPairs == "F1F2":
            _FportPairs = F1F2
        elif _FportPairs == "F3F4" :
            _FportPairs = F3F4
        elif _FportPairs ==  "F5F6":
            _FportPairs = F5F6
        elif _FportPairs =="F7F8":
            _FportPairs = F7F8
        else:
            print(f"invalid pair: {_FportPairs}")
            return 0
        return _FportPairs
    
    
    def __validate_address(self,_DB,_Samtec,_FportPairs):
        addr = 0b0000000000
        if (_DB <0 ) or (_DB > 23) or(_Samtec <0) or  (_Samtec > 3):
            print("valid values: 0<=DB<=23; 0<=Samtec<=3; 0<=Fportpairs<=7")
        _DB = _DB & 0b0000011111
        _Samtec = _Samtec & 0b0000000111
        _FportPairs = _FportPairs & 0b0000000011
        addr = (_DB<<5) | (_Samtec<<2)| _FportPairs
        return addr 

    def measure(self,mode= None, Quadrant = None, DB=0,Samtec=0,FportPairs=None):
        FportPairs = self.__validate_FportsPairs(FportPairs)
        address = self.__validate_address(DB,Samtec,FportPairs)

        try:
                # Times out if the server doesn't respond within 5 seconds
                response = self.session.post(
                    self.url, 
                    json={"mux": Quadrant,
                          "address": address,#0x3e0,
                          "enable": 1,
                          "matrix": self.measurement_configs[mode],
                          },
                           
                    timeout=5
                )
                print(self.url)
                print(f"addr= {address}")
                print(f"addrbin={address:010b}")
                print(self.measurement_configs[mode])
                return response.json()
        except requests.exceptions.Timeout:
               print("Error: The server took too long to respond.")
        except requests.exceptions.RequestException as e:
                print(f"Connection error: {e}")

     

    def diagnostic(self,mode= None, Quadrant = None, DB=0,Samtec=0,FportPairs=None):
        FportPairs = self.__validate_FportsPairs(FportPairs)
        address = self.__validate_address(DB,Samtec,FportPairs)
        try:
                      # Times out if the server doesn't respond within 5 seconds
                response = self.session.post(
                    self.url, 
                    json={"mux": Quadrant,
                          "address": address,#0x3e0,
                          "enable": 1,
                          "matrix": self.diagnostic_configs[mode],
                          },

                        timeout=5
                 )
                print(self.url)
                print(f"addr= {address}")
                print(f"addrbin={address:010b}")
                print(self.measurement_configs[mode])
                return response.json()
        except requests.exceptions.Timeout:
               print("Error: The server took too long to respond.")
        except requests.exceptions.RequestException as e:
                print(f"Connection error: {e}")  
                return 1      
      
    




ctrl = RTController("http://192.168.1.50:5000/")

import time
import json
from datetime import datetime



count = 0   
while(True):
    while(True):
        response = ctrl.measure(mode="config_1",Quadrant=1, DB=1, Samtec = 1, FportPairs= "F5F6")
        #time.sleep(1)
        if response != None and response['status'] == 'OK':
            break
              

    #ctrl.diagnostic(mode="config_1",DB=1, Samtec=1,FportPairs="F5F6")
    count = count +1
    status = response.get("status") if isinstance(response, dict) else None
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
    with open("response_log.txt", "a", buffering=1) as log_file:
        log_file.write(f"{timestamp}, count={count}, status={status}\n")
    
    print(f"count-->{count}" )
    print(f"response :: {response}")
    if response !=None and response['status'] == 'WRONG COMMAND':
         break
    

if 0:

    try:
        # Times out if the server doesn't respond within 5 seconds
        response = requests.post(
            "http://192.168.1.50:5000/", 
            json={"mux": 4,
                  "address": 1023,#0x3e0,
                  "enable": 1,
                  "matrix": {
                      "1":3,
                      "2": 1,
                  },
                  }, 
            timeout=5
        )
        print(response.json())
    except requests.exceptions.Timeout:
        print("Error: The server took too long to respond.")
    except requests.exceptions.RequestException as e:
        print(f"Connection error: {e}")
