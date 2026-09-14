.PHONY: release debug debugaddr debugthread clean

release:
	cmake -B build/release \
		-DCMAKE_BUILD_TYPE=Release \
		-DSANITIZER=none
	cmake --build build/release -j$(nrpoc)

debug:
	cmake -B build/debug \
		-DCMAKE_BUILD_TYPE=Debug \
		-DSANITIZER=none
	cmake --build build/debug -j$(nrpoc)

debugaddr:
	cmake -B build/debugaddr \
		-DCMAKE_BUILD_TYPE=Debug \
		-DSANITIZER=address
	cmake --build build/debugaddr -j$(nrpoc)

debugthread:
	cmake -B build/debugthread \
		-DCMAKE_BUILD_TYPE=Debug \
		-DSANITIZER=thread
	cmake --build build/debugthread -j$(nproc)

clean:
	rm -rf build
