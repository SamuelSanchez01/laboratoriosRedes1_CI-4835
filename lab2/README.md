# Lab II · Concurrent servers, names and UDP

Redes de Computadoras I (CI-4835) · C++17 on Linux.

| File | What it does |
|---|---|
| `socket.hpp` | `Socket` class from Lab I: closes the fd in its destructor (RAII) |
| `net.hpp` | `send_all`, `LineReader` (whole lines over TCP) and `addr_to_string` |
| `server.cpp` | Concurrent TCP echo server: one thread per client, IPv6 + IPv4 (dual stack) |
| `client.cpp` | TCP client that accepts host names (`getaddrinfo`) and tries every address |
| `udp_echo.cpp` | UDP echo server: `recvfrom` / `sendto`, no connections |
| `dnsq.cpp` | Mini `dig`: builds a DNS query by hand and sends it over UDP to port 53 |

## Build

```bash
make            # builds server, client, udp_echo and dnsq
make clean
```

## Run

```bash
# Concurrent TCP server: open several clients at the same time
./server 5000
./client localhost 5000
./client 127.0.0.1 5000
./client ::1 5000
ss -tnp | grep 5000              # one ESTAB line per client

# UDP echo
./udp_echo 6000
nc -u 127.0.0.1 6000             # each line you type is one datagram

# DNS by hand over UDP
./dnsq www.usb.ve                # A record, server from /etc/resolv.conf
./dnsq www.usb.ve AAAA           # IPv6
./dnsq www.usb.ve A 8.8.8.8      # like: dig @8.8.8.8 www.usb.ve
dig www.usb.ve                   # compare
```

Wireshark filters: `tcp.port == 5000`, `udp.port == 6000`, `dns`.
To see `dnsq` leave your machine, ask `8.8.8.8` and capture on your Wi-Fi/Ethernet interface (or `any`).
