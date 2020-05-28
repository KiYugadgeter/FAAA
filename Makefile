fsys: fsys.cpp fsys.hpp
	clang++ -g -o fsys fsys.cpp

fs: fs.nasm
	nasm -o fs -f bin fs.nasm


.PHONY: debug
debug: 
	clang++ -g -o fsys fsys.cpp

.PHONY: clean
clean:
	rm fs
	rm fsys

