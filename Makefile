## you have command.cc command.h tokenizer.cc tokenizer.h you need compile them and link
## them to one object file called  : "myshell" 
## so we should run make , that will create our object myshell ,
## ./myshell should run your application 

CXX      = g++
CXXFLAGS = -std=c++17 -Wall -g

all: myshell

myshell: command.o tokenizer.o
	$(CXX) $(CXXFLAGS) -o myshell command.o tokenizer.o

%.o: %.cc command.h tokenizer.h
	$(CXX) $(CXXFLAGS) -c $<

clean:
	rm -f *.o myshell shell.log