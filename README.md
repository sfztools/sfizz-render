# sfizz-render

**sfizz-render now is bundled in https://github.com/sfztools/sfizz**

## Building and installing

`sfizz-render` requires the `sndfile` library ; on Debian-based systems, you can install `libsndfile1-dev`.
It also requires `sfizz` installed as a shared library.
On Arch you have AUR packages available for this, or install it from source following the information in (https://github.com/sfztools/sfizz).
To build the release from source use
```
git clone --recursive https://github.com/sfztools/sfizz-render.git
cd sfizz-render
make
```

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
Render a midi file through an SFZ file using the sfizz library.
Usage:
  sfizz-render [OPTION...]

      --sfz arg           SFZ file
      --midi arg          Input midi file
      --wav arg           Output wav file
  -b, --blocksize arg     Block size for the sfizz callbacks
  -s, --samplerate arg    Output sample rate
  -t, --track arg         Track number to use
      --oversampling arg  Internal oversampling factor
  -v, --verbose           Verbose output
      --use-eot           End the rendering at the last End of Track Midi
                          message
  -h, --help              Show help
```
