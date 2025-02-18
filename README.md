cmake -G "Unix Makefiles" ..
make

auto start = std::chrono::high_resolution_clock::now();

auto end = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
std::cout << "Execution time1 :" << duration.count() << "microseconds" << std::endl;