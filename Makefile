#Name: Nicholas Lingad
#Email: lingadnicholas@g.ucla.edu
#ID: 605284477

default: 
	if uname -a | grep -q "armv7l"; then \
		gcc -Wall -Wextra -lmraa -lm -o lab4c_tcp lab4c_tcp.c; \
		gcc -Wall -Wextra -lmraa -lm -lssl -lcrypto -o lab4c_tls lab4c_tls.c; \
	else \
		gcc -Wall -Wextra -DDUMMY -lm -o lab4c_tcp lab4c_tcp.c; \
		gcc -Wall -Wextra -DDUMMY -lm -lssl -lcrypto -o lab4c_tls lab4c_tls.c; \
	fi 

clean:
	rm -f lab4b lab4c-605284477.tar.gz 

dist:
	tar -czvf lab4c-605284477.tar.gz README lab4c_tcp.c lab4c_tls.c Makefile

