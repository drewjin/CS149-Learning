cd /home/drew/Desktop/Parallel Computing-CS 149/parallel_prefix_sum
cmake -S . -B ./build -DCMAKE_BUILD_TYPE=Debug
cd build
make -j24