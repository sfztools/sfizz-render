// Copyright (c) 2019, Paul Ferrand
// All rights reserved.

// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:

// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.

// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
// ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "sfizz.hpp"
#include <sndfile.hh>
#include "MidiFile.h"
#include "absl/flags/parse.h"
#include "absl/flags/flag.h"
#include "absl/flags/usage.h"
#include "ghc/filesystem.hpp"
#include <iostream>

constexpr int buildAndCenterPitch(uint8_t firstByte, uint8_t secondByte)
{
    return (int)(((unsigned int)secondByte << 7) + (unsigned int)firstByte) - 8192;
}

ABSL_FLAG(std::string, oversampling, "x1", "Internal oversampling factor");
ABSL_FLAG(int, blocksize, 1024, "Block size for the sfizz callbacks");
ABSL_FLAG(int, samplerate, 48000, "Output sample rate");
ABSL_FLAG(int, track, -1, "Track number to use");
ABSL_FLAG(std::string, sfz, "", "Internal oversampling factor");
ABSL_FLAG(std::string, wav, "", "Output wav file");
ABSL_FLAG(std::string, midi, "", "Output wav file");
ABSL_FLAG(bool, verbose, false, "Verbose output");

int main(int argc, char** argv)
{
    absl::SetProgramUsageMessage("Render a midi file through an SFZ file using the sfizz library.");
    [[maybe_unused]] auto params = absl::ParseCommandLine(argc, argv);

    const auto verbose = absl::GetFlag(FLAGS_verbose);
#define LOG_ERROR(ostream) std::cerr  << ostream << '\n'
#define LOG_INFO(ostream) if (verbose) { std::cout << ostream << '\n'; }

    if (absl::GetFlag(FLAGS_sfz).empty()) {
        LOG_ERROR("Please specify an SFZ file using --sfz");
        std::exit(-1);
    }

    if (absl::GetFlag(FLAGS_wav).empty()) {
        LOG_ERROR("Please specify an output file using --wav");
        std::exit(-1);
    }

    if (absl::GetFlag(FLAGS_midi).empty()) {
        LOG_ERROR("Please specify a MIDI file using --midi");
        std::exit(-1);
    }

    fs::path sfzPath  = fs::current_path() / absl::GetFlag(FLAGS_sfz);
    fs::path outputPath  = fs::current_path() / absl::GetFlag(FLAGS_wav);
    fs::path midiPath  = fs::current_path() / absl::GetFlag(FLAGS_midi);

    if (!fs::exists(sfzPath) || !fs::is_regular_file(sfzPath)) {
        LOG_ERROR("SFZ file " << sfzPath.string() << " does not exist or is not a regular file");
        std::exit(-1);
    }

    if (!fs::exists(midiPath) || !fs::is_regular_file(midiPath)) {
        LOG_ERROR("MIDI file " << midiPath.string() << " does not exist or is not a regular file");
        std::exit(-1);
    }

    if (fs::exists(outputPath)) {
        LOG_INFO("Output file " << outputPath.string() << " already exists and will be erased.");
    }

    const auto oversampling = absl::GetFlag(FLAGS_oversampling);
    const auto factor = [&]() {
        if (oversampling == "x1") return sfz::Oversampling::x1;
        if (oversampling == "x2") return sfz::Oversampling::x2;
        if (oversampling == "x4") return sfz::Oversampling::x4;
        if (oversampling == "x8") return sfz::Oversampling::x8;

        LOG_ERROR("Unknown oversampling factor " << oversampling);
        std::exit(-1);

        return sfz::Oversampling::x1;
    }();

    LOG_INFO("SFZ file:    " << sfzPath.string());
    LOG_INFO("MIDI file:   " << midiPath.string());
    LOG_INFO("Output file: " << outputPath.string());
    switch(factor) {
    case sfz::Oversampling::x1:
        LOG_INFO("Oversampling factor: " << "x1");
        break;
    case sfz::Oversampling::x2:
        LOG_INFO("Oversampling factor: " << "x2");
        break;
    case sfz::Oversampling::x4:
        LOG_INFO("Oversampling factor: " << "x4");
        break;
    case sfz::Oversampling::x8:
        LOG_INFO("Oversampling factor: " << "x8");
        break;
    }
    const auto blockSize = absl::GetFlag(FLAGS_blocksize);
    LOG_INFO("Block size: " << blockSize);

    const auto sampleRate = absl::GetFlag(FLAGS_samplerate);
    LOG_INFO("Sample rate: " << sampleRate);
 
    sfz::Synth synth;
    synth.setSamplesPerBlock(blockSize);
    synth.setSampleRate(sampleRate);
    synth.enableFreeWheeling();
    if (!synth.loadSfzFile(sfzPath)) {
        LOG_ERROR("There was an error loading the SFZ file.");
        std::exit(-1);
    }
    LOG_INFO(synth.getNumRegions() << " regions in the SFZ.");

    const auto trackNumber = absl::GetFlag(FLAGS_track);
    smf::MidiFile midiFile { midiPath.string() };
    LOG_INFO(midiFile.getNumTracks() << " tracks in the SMF.");
    if (trackNumber > midiFile.getNumTracks()) {
        LOG_ERROR("The track number " <<  trackNumber << " requested does not exist in the SMF file.");
        std::exit(-1);
    }

    if (trackNumber < 1) {
        midiFile.joinTracks();
    } else {
        LOG_INFO("-- Rendering only track number " <<  trackNumber);
    }
    
    const auto trackIdx = trackNumber < 1 ? 0 : trackNumber - 1;

    midiFile.doTimeAnalysis();

    SndfileHandle outputFile (outputPath, SFM_WRITE, SF_FORMAT_WAV | SF_FORMAT_PCM_16, 2, sampleRate);
    if (outputFile.error() != 0) {
        LOG_ERROR("Error writing out the wav file: " << outputFile.strError());
        std::exit(-1);
    }

    int evIdx { 0 };
    int nextBlockSentinel { blockSize };
    auto sampleRateDouble = static_cast<double>(sampleRate);
    double blockStartTime { 0 };
    double blockSizeInSeconds { blockSize / sampleRateDouble };
    int numFramesWritten { 0 };
    sfz::AudioBuffer<float> buffer (2, blockSize);
    sfz::Buffer<float> interleavedBuffer (2 * blockSize);

    auto leftSpan = buffer.getSpan(0);
    auto rightSpan = buffer.getSpan(1);
    auto interleavedSpan = absl::MakeSpan(interleavedBuffer);
    while (evIdx < midiFile.getNumEvents(trackIdx)) {
        const auto midiEvent = midiFile.getEvent(trackIdx, evIdx);
        const auto sampleIndex = static_cast<int>(midiEvent.seconds * sampleRateDouble);
        if (sampleIndex > nextBlockSentinel) {
            synth.renderBlock(buffer);
            sfz::writeInterleaved<float>(leftSpan, rightSpan, interleavedSpan);
            numFramesWritten += outputFile.writef(interleavedSpan.data(), blockSize);
            // Avoid absorption, if any, by keeping the counter integer until the end
            blockStartTime = static_cast<double>(nextBlockSentinel) / sampleRateDouble; 
            nextBlockSentinel += blockSize;
        } else {
            const auto delay = static_cast<int>((midiEvent.seconds - blockStartTime) * sampleRateDouble); 
            if (midiEvent.isNoteOn() && midiEvent.getVelocity() > 0) {
                synth.noteOn(delay, midiEvent.getKeyNumber(), midiEvent.getVelocity());
            }
            else if (midiEvent.isNoteOff() || (midiEvent.isNoteOn() && midiEvent.getVelocity() == 0)) {
                synth.noteOff(delay, midiEvent.getKeyNumber(), midiEvent.getVelocity());
            }
            else if (midiEvent.isController()) {
                synth.noteOn(delay, midiEvent.getControllerNumber(), midiEvent.getControllerValue());
            }
            else if (midiEvent.isPitchbend()) {
                synth.pitchWheel(delay, buildAndCenterPitch(midiEvent[1], midiEvent[2]));
            }
            else {
                LOG_INFO("Unhandled event at delay " << delay << " " << +midiEvent[0] << " " << +midiEvent[1] ) 
            }
            
            evIdx++;
        }
    }

    auto averagePower = sfz::meanSquared<float>(interleavedBuffer);
    while (averagePower > 1e-12f || nextBlockSentinel == blockSize) {
        synth.renderBlock(buffer);
        sfz::writeInterleaved<float>(leftSpan, rightSpan, interleavedSpan);
        numFramesWritten += outputFile.writef(interleavedSpan.data(), blockSize);
        blockStartTime = static_cast<double>(nextBlockSentinel) / sampleRateDouble; 
        nextBlockSentinel += blockSize;
        averagePower = sfz::meanSquared<float>(interleavedBuffer);
    }

    outputFile.writeSync();
    LOG_INFO("Wrote " << blockStartTime << " seconds of sound data in"
              << outputPath.string() << " (" << numFramesWritten << " frames)");

    return 0;
}