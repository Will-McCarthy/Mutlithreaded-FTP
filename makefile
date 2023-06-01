# CSCI 4780 Spring 2021 Project 1
# Jisoo Kim, Will McCarthy, Tyler Scalzo

GCC=g++
FLAGS=-lpthread
CLTPROG=myftp
SRVPROG=myftpserver

$(CLTPROG): myftp.o
	$(GCC) -o $(CLTPROG) myftp.o ${FLAGS}

$(SRVPROG): myftpserver.o
	$(GCC) -o $(SRVPROG) myftpserver.o ${FLAGS}

myftp.o: myftp.cpp
	$(GCC) -c myftp.cpp ${FLAGS}

myftpserver.o: myftpserver.cpp
	$(GCC) -c myftpserver.cpp ${FLAGS}

all: $(CLTPROG) $(SRVPROG)

clean:
	rm -rf $(CLTPROG) $(SRVPROG) *.o