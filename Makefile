
default: 
	if uname -a | grep -q "armv7l"; then \
		gcc -Wall -Wextra -lmraa -lm -o iot_tcp iot_tcp.c; \
		gcc -Wall -Wextra -lmraa -lm -lssl -lcrypto -o iot_tls iot_tls.c; \
	else \
		gcc -Wall -Wextra -DDUMMY -lm -o iot_tcp iot_tcp.c; \
		gcc -Wall -Wextra -DDUMMY -lm -lssl -lcrypto -o iot_tls iot_tls.c; \
	fi 

clean:
	rm -f lab4b iot.tar.gz 

dist:
	tar -czvf iot.tar.gz README iot_tcp.c iot_tls.c Makefile

