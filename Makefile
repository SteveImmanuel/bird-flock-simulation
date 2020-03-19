COMPILER = g++
FLAG = -lGL -lGLU -lglut -lGLEW

main: main.cpp
	$(COMPILER) -Wall main.cpp $(FLAG) -o main  
clean:
	rm main

