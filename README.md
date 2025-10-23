\# Mini HTTP Server in C



A lightweight, concurrent HTTP/1.1 web server implemented in pure C.



\## Features



\- \*\*Concurrent Client Handling\*\*: Fork-based parallelism

\- \*\*HTTP/1.1 Support\*\*: GET method implementation

\- \*\*MIME Type Detection\*\*: Automatic content-type headers

\- \*\*Security\*\*: Basic path traversal protection

\- \*\*Efficient File Serving\*\*: Zero-copy file transmission

\- \*\*Error Handling\*\*: Proper HTTP status codes (200, 404, 500, etc.)



\## 🛠️ Build Instructions



```bash

\# Compile with debug symbols

gcc -g -o server server.c



\# Or compile with optimizations

gcc -O2 -o server server.c



\# Compile with all warnings enabled

gcc -Wall -Wextra -Wpedantic -o server server.c

