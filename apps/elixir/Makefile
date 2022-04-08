


all:
	mix deps.get
	mix deps.compile
	mix compile

build_lib: ../../_build/libbacnet-stack.so

../../_build/libbacnet-stack.so:
	cd ../../ && ./lib_build.sh
 
 
