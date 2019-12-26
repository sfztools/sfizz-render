# sfizz-render

## Building and installing

You should build this repository from source.
It uses the FetchContent addon to CMake and thus it requires a connection to various git repositories on the configure step.
It requires the `sndfile` library; on Debian-based systems, you can install `libsndfile1-dev`.
To build the release from source use
```
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```
Eventually you can use `sudo make install` to install the binary.

## Usage

You have to specify 3 elements to `sfizz-render`:
- A MIDI/SMF file
- An SFZ file
- An output WAV file
The output file can only be a stereo WAV file for now, not because of technical reasons because `libsndfile` is very versatile, but because I did not want to multiply the possible flags.

The basic usage is
```
sfizz-render --wav wav_file.wav --sfz sfz_file.sfz --midi midi_file.mid
```

The complete list of command line options is:
```
sfizz-render: Render a midi file through an SFZ file using the sfizz library.

  Flags from home/paul/source/midi-sfizz/main.cpp:
    -midi (Output wav file); default: "";
    -wav (Output wav file); default: "";
    -sfz (Internal oversampling factor); default: "";
    -oversampling (Internal oversampling factor); default: "x1";
    -blocksize (Block size for the sfizz callbacks); default: 1024;
    -samplerate (Output sample rate); default: 48000;
    -track (Track number to use); default: -1;
    -verbose (Verbose output); default: false;
```