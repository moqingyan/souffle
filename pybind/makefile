PYTHON3 = python3
GOMP_LIB_SRC="@rpath/libgomp.1.dylib"
GOMP_LIB_DST="/usr/local/Cellar/gcc/10.1.0/lib/gcc/10/libgomp.1.dylib"

LIBSOUFFLE_OBJS = \
	libsouffle_la-AstComponentChecker.o \
	libsouffle_la-AstTypeAnalysis.o \
	libsouffle_la-InterpreterIndex.o \
	libsouffle_la-RamTransforms.o \
	libsouffle_la-AstTypeEnvironmentAnalysis.o \
 	libsouffle_la-InterpreterRelation.o \
 	libsouffle_la-ReorderLiteralsTransformer.o \
	libsouffle_la-AstGroundAnalysis.o \
 	libsouffle_la-AstUtils.o \
 	libsouffle_la-MagicSet.o \
 	libsouffle_la-ResolveAliasesTransformer.o \
	libsouffle_la-AstIOTypeAnalysis.o \
 	libsouffle_la-AuxArityAnalysis.o \
 	libsouffle_la-MinimiseProgramTransformer.o \
 	libsouffle_la-SrcLocation.o \
	libsouffle_la-AstParserUtils.o \
 	libsouffle_la-ComponentInstantiationTransformer.o \
 	libsouffle_la-ParserDriver.o \
 	libsouffle_la-Synthesiser.o \
	libsouffle_la-AstPragmaChecker.o \
 	libsouffle_la-ComponentLookupAnalysis.o \
 	libsouffle_la-PrecedenceGraph.o \
 	libsouffle_la-SynthesiserRelation.o \
	libsouffle_la-AstProfileUse.o \
 	libsouffle_la-DebugReport.o \
 	libsouffle_la-ProvenanceTransformer.o \
 	libsouffle_la-TypeSystem.o \
	libsouffle_la-AstSemanticChecker.o \
 	libsouffle_la-DebugReporter.o \
 	libsouffle_la-RamComplexityAnalysis.o \
 	libsouffle_la-parser.o \
	libsouffle_la-AstTransformer.o \
 	libsouffle_la-Global.o \
 	libsouffle_la-RamIndexAnalysis.o \
 	libsouffle_la-scanner.o \
	libsouffle_la-AstTransforms.o \
 	libsouffle_la-InlineRelationsTransformer.o \
 	libsouffle_la-RamLevelAnalysis.o \
	libsouffle_la-AstTranslator.o \
 	libsouffle_la-InterpreterEngine.o \
 	libsouffle_la-RamTransformer.o \
 	
LIBSOUFFLE_OBJS_IN_DIR = $(patsubst %.o, ../src/%.o, $(LIBSOUFFLE_OBJS))

LIBSOUFFLE_SHARED_OBJS_IN_DIR = $(patsubst %.o, %.so, $(LIBSOUFFLE_OBJS_IN_DIR))

new_entry: update $(LIBSOUFFLE_SHARED_OBJS_IN_DIR)
	g++ -std=c++17 -shared -O3 -Wall -fPIC \
		`$(PYTHON3) -m pybind11 --includes` \
		-I/home/jianih/local/anaconda3/include \
		-I../src \
		-I../ \
		-Wl,-rpath,/home/jianih/local/anaconda3/lib \
		-Wl,/home/jianih/local/anaconda3/lib/libffi.so \
		-L/home/jianih/.local/lib \
		-L/home/jianih/local/anaconda3/lib \
		-lffi -lsqlite3 -lz \
		PySouffle.cpp \
		$(LIBSOUFFLE_OBJS_IN_DIR) \
		-o PySouffle`$(PYTHON3)-config --extension-suffix`

# all: update obj link substitute_gomp
# all: update obj debug
all: update $(LIBSOUFFLE_SHARED_OBJS_IN_DIR) obj link

update:
	make -C ..

obj:
	g++ \
		-O3 \
		`$(PYTHON3) -m pybind11 --includes` \
		-I/home/jianih/local/anaconda3/include \
		-I../src \
		-I.. \
		-L/home/jianih/local/anaconda3/lib -lffi \
		-lsqlite3\
		-lz \
		-std=c++17 -shared -O3 -Wall -fPIC \
		-c ../src/PySouffle.cpp \
		-o PySouffle.o 

%.so: %.o
	g++ -shared $< -o $@

# c++ -O3 -Wall -shared -std=c++11 -fPIC `python3 -m pybind11 --includes` example.cpp -o example`python3-config --extension-suffix`

link: 
	g++ \
		`$(PYTHON3) -m pybind11 --includes` \
		-Wall\
		-std=c++17 -shared -O3 -Wall -fPIC \
		-I../src \
		-I.. \
		-L/home/jianih/.local/lib/ \
		-L/usr/lib/x86_64-linux-gnu/ \
		-L/home/jianih/local/anaconda3/lib/\
		-lpthread -lz -lncurses -lgomp -lffi -lsqlite3 -fopenmp\
		-v \
 		-o PySouffle`$(PYTHON3)-config --extension-suffix` \
		$(LIBSOUFFLE_OBJS_IN_DIR) PySouffle.o \
		/home/jianih/local/anaconda3/lib/libffi.so \
		/home/jianih/local/anaconda3/lib/libsqlite3.so\
		/home/jianih/local/anaconda3/lib/libz.so
		# -L/home/jianih/local/anaconda3/lib \
		# -lffi -lsqlite3 -lz

debug:
	g++ \
		-I/Users/moqingyan/Tools/anaconda3/include/python3.8 \
		-I/usr/local/opt/libffi/include \
		-I/usr/local/Cellar/sqlite/3.32.3/include \
		-I../src \
		-I.. \
		-g \
		-L/Users/moqingyan/Tools/anaconda3/lib/ \
		-L/usr/local/opt/libffi/lib/ -lffi\
		-L/usr/local/Cellar/sqlite/3.32.3/lib -lsqlite3 \
		-lz \
		-std=c++17 -O0 -Wall -undefined dynamic_lookup -fPIC \
		-o PySouffle.out \
		$(LIBSOUFFLE_OBJS_IN_DIR) PySouffle.o

substitute_gomp:
	install_name_tool -change ${GOMP_LIB_SRC} ${GOMP_LIB_DST} PySouffle`$(PYTHON3)-config --extension-suffix`

check_libs:
	otool -L PySouffle`$(PYTHON3)-config --extension-suffix`

echo:
	echo $(LIBSOUFFLE_OBJS_IN_DIR)