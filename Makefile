all: sample2D

sample2D: Sample_GL3_2D.cpp glad.c
	g++ -o sample2D Sample_GL3_2D.cpp glad.c -lpthread -lGL -lglfw -ldl -lao -lm

clean:
	rm sample2D
