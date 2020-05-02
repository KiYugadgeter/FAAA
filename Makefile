fsys: fsys.cpp fsys.hpp
	g++ -g -o fsys fsys.cpp


.PHONY: sanitize
sanitize: 
	g++ -fsanitize=address -g -o sani fsys.cpp



