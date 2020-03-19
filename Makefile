COMPILER = g++
FLAG = -lGL -lGLU -lglut -lGLEW

main: main.cpp
	$(COMPILER) main.cpp $(FLAG) -o main  
clean:
	rm main

