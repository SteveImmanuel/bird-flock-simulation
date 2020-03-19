COMPILER = g++
FLAG = -lGL -lGLU -lglut -lGLEW

main: 
	$(COMPILER) main.cpp $(FLAG) -o main  
clean:
	rm main

