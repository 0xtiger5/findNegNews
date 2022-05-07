app : myserver.cc
	g++ -std=c++11 -I /opt/sogou/include/ -o myserver  myserver.cc  /opt/sogou/lib/libworkflow.a -lssl -lpthread -lcrypto -l curl
.PHONY:clean
clean:
	rm -rf myserver