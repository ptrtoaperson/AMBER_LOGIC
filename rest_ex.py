import requests
session = requests.Session()

resp = session.post("http://192.168.1.50:5000", json={"Quadrant":1,
                                                "DB":1,
                                                "Samtec":1,
                                                "FportPairs":"F5F6",
                                                "enable":1,
                                                "mode":"measurement",
                                                "config":"config_1"})


response = resp.json()

print(response)