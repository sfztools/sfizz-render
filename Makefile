name = sfizz-render
src = main.cpp
obj = $(src:.cpp=.o)

LDFLAGS = `pkg-config --libs sndfile sfizz` -Lmidifile/lib -l:libmidifile.a
CXXFLAGS += -Imidifile/include -std=c++17 -O3

$(name): $(obj)
	$(CXX) -o $@ $^ $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(obj) $(name)

.PHONY: midifile
midifile:
	$(MAKE) -C midifile library