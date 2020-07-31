# Compile Souffle

```
g++-10 -I/Users/moqingyan/Tools/anaconda3/include/python3.8  -I/usr/local/opt/libffi/include -I.
-I.. -std=c++17 -L/Users/moqingyan/Tools/anaconda3/lib/ --shared -O3 -Wall -undefined dynamic_lookup
-fPIC (python3 -m pybind11 --includes) PySouffle.cpp -o PySouffle(python3-config --extension-suffix)
```

# Generate PySouffle.o File

```
g++-10 -I/Users/moqingyan/Tools/anaconda3/include/python3.8  -I/usr/local/opt/libffi/include -I. -I.. -std=c++17 -L/Users/moqingyan/Tools/anaconda3/lib/ --shared -O3 -Wall -undefined dynamic_lookup -fPIC (python3 -m pybind11 --includes) -c PySouffle.cpp
```