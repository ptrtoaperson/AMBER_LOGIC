import requests

try:
    # Times out if the server doesn't respond within 5 seconds
    response = requests.post(
        "http://192.168.1.50:5000/calculate", 
        json={"mux": 4,
              "address": 0x3e0,
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
