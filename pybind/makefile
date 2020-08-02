PYTHON3 = /Users/moqingyan/Tools/anaconda3/bin/python3
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

all: update obj link substitute_gomp
# all: update obj debug

update:
	make -C ..

obj:
	g++-10 \
	    --shared \
		-I/Users/moqingyan/Tools/anaconda3/include/python3.8 \
		-I/usr/local/opt/libffi/include \
		-I/usr/local/Cellar/sqlite/3.32.3/include \
		`$(PYTHON3) -m pybind11 --includes` \
		-I../src \
		-I.. \
		-std=c++17 --shared -O3 -Wall -undefined dynamic_lookup -fPIC \
		-L/Users/moqingyan/Tools/anaconda3/lib/ \
		-L/usr/local/Cellar/sqlite/3.32.3/lib -lsqlite3 \
		-L/usr/local/opt/libffi/lib/ -lffi \
		-lz \
		-c ../src/PySouffle.cpp \
		-o PySouffle.o

link: 
	g++-10 \
		--shared \
		-I/Users/moqingyan/Tools/anaconda3/include/python3.8 \
		-I/usr/local/opt/libffi/include \
		-I/usr/local/Cellar/sqlite/3.32.3/include \
		-I../src \
		-I.. \
		-L/Users/moqingyan/Tools/anaconda3/lib/ \
		-L/usr/local/opt/libffi/lib/ -lffi\
		-L/usr/local/Cellar/sqlite/3.32.3/lib -lsqlite3 \
		-lz \
		-std=c++17 --shared -O3 -Wall -undefined dynamic_lookup -fPIC \
		`$(PYTHON3) -m pybind11 --includes` \
 		-o PySouffle`$(PYTHON3)-config --extension-suffix` \
		$(LIBSOUFFLE_OBJS_IN_DIR) PySouffle.o

debug:
	g++-10 \
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