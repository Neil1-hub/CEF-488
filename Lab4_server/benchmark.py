

import threading
import urllib.request
import time

URL = "http://localhost:8080/index.html"
CONCURRENT_REQUESTS = 100

def make_request():
    try:
        with urllib.request.urlopen(URL, timeout=1) as response:
            response.read()
    except Exception:
        pass

print("Starting benchmark with 100 concurrent requests...")
start_time = time.time()

threads = []
for _ in range(CONCURRENT_REQUESTS):
    t = threading.Thread(target=make_request)
    threads.append(t)
    t.start()

for t in threads:
    t.join()

elapsed = time.time() - start_time
rps = CONCURRENT_REQUESTS / elapsed
print(f"Completed in: {elapsed:.4f} seconds")
print(f"Throughput: {rps:.2f} Requests/Second")
