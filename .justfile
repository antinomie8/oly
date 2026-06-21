release:
	@cmake -DCMAKE_BUILD_TYPE=Release -B build/Release
	@cmake --build build/Release --parallel

relwithdeb:
	@cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -B build/RelWithDebInfo
	@cmake --build build/RelWithDebInfo --parallel

debug:
	@cmake -DCMAKE_BUILD_TYPE=Debug -B build/Debug
	@cmake --build build/Debug

dbg: debug
	cp build/Debug/oly ~/.local/bin/
	oly --version

install: release
	cp build/Release/oly ~/.local/bin/
	oly --version

lint:
	@-run-clang-tidy                           \
		-p build                                 \
		-source-filter='^.*/oly/(src|include).*' \
		-quiet

test: debug
	-./test.lua
