.PHONY: release debug debugaddr debugthread clean

release:
	cmake -S . -B build/release \
		-DCMAKE_BUILD_TYPE=Release \
		-DSANITIZER=none
	cmake --build build/release -j

debug:
	cmake -S . -B build/debug \
		-DCMAKE_BUILD_TYPE=Debug \
		-DSANITIZER=none
	cmake --build build/debug -j

debugaddr:
	cmake -S . -B build/debugaddr \
		-DCMAKE_BUILD_TYPE=Debug \
		-DSANITIZER=address
	cmake --build build/debugaddr -j

debugthread:
	cmake -S . -B build/debugthread \
		-DCMAKE_BUILD_TYPE=Debug \
		-DSANITIZER=thread
	cmake --build build/debugthread -j

clean:
	rm -rf build
