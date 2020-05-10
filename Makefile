fsys: fsys.cpp fsys.hpp
	g++ -g -o fsys fsys.cpp

fs: fs.nasm
	nasm -o fs -f bin fs.nasm


.PHONY: debug
debug: 
	g++ -g -o fsys fsys.cpp

.PHONY: clean
clean:
	rm fs fsys

