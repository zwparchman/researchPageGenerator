all :
	./main.py

index.html :
	./main.py

view : all
	google-chrome index.html
