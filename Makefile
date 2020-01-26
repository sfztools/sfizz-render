name = sfizz-render
src = main.cpp
obj = $(src:.cpp=.o)

LDFLAGS = `pkg-config --libs sndfile sfizz` -Lmidifile/lib -l:libmidifile.a
CXXFLAGS += -Imidifile/include -std=c++17 -O3

$(name): $(obj) libmidifile
	$(CXX) -o $@ $(obj) $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(obj) $(name)

.PHONY: libmidifile
libmidifile:
	$(MAKE) -C midifile library