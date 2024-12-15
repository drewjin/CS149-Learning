build_type=$1
cmake -S . -B ./build -DCMAKE_BUILD_TYPE=$build_type
cmake --build ./build -j $(nproc)
