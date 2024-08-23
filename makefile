.PHONY:
main: main.cc
	g++ -std=c++11 -g  main.cc -o main
clean:
	rm -f main
