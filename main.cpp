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

#include <sfizz.hpp>
#include <sndfile.hh>
#include "MidiFile.h"
#include "cxxopts.hpp"
#include <filesystem>
#include <iostream>
#define LOG_ERROR(ostream) std::cerr  << ostream << '\n'
#define LOG_INFO(ostream) if (verbose) { std::cout << ostream << '\n'; }

namespace fs = std::filesystem;

constexpr int buildAndCenterPitch(uint8_t firstByte, uint8_t secondByte)
{
    return (int)(((unsigned int)secondByte << 7) + (unsigned int)firstByte) - 8192;
}

float meanSquared(const std::vector<float>& array)
{
    float power = 0;
    for (auto& value: array)
        power += value * value;
    power /= array.size();
    return power;
}

void writeInterleaved(const std::vector<float>& left, const std::vector<float>& right, std::vector<float>& output)
{
    const auto inputSamples = std::min(left.size(), right.size());
    const auto numSamples = std::min(inputSamples, output.size() / 2);
    for (size_t i = 0; i < numSamples; ++i) {
        output[2*i] = left[i];
        output[2*i+1] = right[i];
    }
}

int main(int argc, char** argv)
{
    cxxopts::Options options("sfizz-render", "Render a midi file through an SFZ file using the sfizz library.");

    int blockSize { 1024 };
    int sampleRate { 48000 };
    int trackNumber { -1 };
    bool verbose { false };
    bool help { false };
    bool useEOT { false };
    int oversampling { 1 };

    options.add_options()
        ("sfz", "SFZ file", cxxopts::value<std::string>())
        ("midi", "Input midi file", cxxopts::value<std::string>())
        ("wav", "Output wav file", cxxopts::value<std::string>())
        ("b,blocksize", "Block size for the sfizz callbacks", cxxopts::value(blockSize))
        ("s,samplerate", "Output sample rate", cxxopts::value(sampleRate))
        ("t,track", "Track number to use", cxxopts::value(trackNumber))
        ("oversampling", "Internal oversampling factor", cxxopts::value(oversampling))
        ("v,verbose", "Verbose output", cxxopts::value(verbose))
        ("use-eot", "End the rendering at the last End of Track Midi message", cxxopts::value(useEOT))
        ("h,help", "Show help", cxxopts::value(help))
    ;
    auto params = [&]() {
        try { return options.parse(argc, argv); }
        catch (std::exception& e) { 
            LOG_ERROR(e.what());
            std::exit(-1);
        }
    }();

    if (help) {
        std::cout << options.help();
        std::exit(0);
    }


    if (params.count("sfz") != 1) {
        LOG_ERROR("Please specify a single SFZ file using --sfz");
        std::exit(-1);
    }

    if (params.count("wav") != 1) {
        LOG_ERROR("Please specify an output file using --wav");
        std::exit(-1);
    }

    if (params.count("midi") != 1) {
        LOG_ERROR("Please specify a MIDI file using --midi");
        std::exit(-1);
    }

    fs::path sfzPath  = fs::current_path() / params["sfz"].as<std::string>();
    fs::path outputPath  = fs::current_path() / params["wav"].as<std::string>();
    fs::path midiPath  = fs::current_path() / params["midi"].as<std::string>();

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

    LOG_INFO("SFZ file:    " << sfzPath.string());
    LOG_INFO("MIDI file:   " << midiPath.string());
    LOG_INFO("Output file: " << outputPath.string());
    LOG_INFO("Block size: " << blockSize);
    LOG_INFO("Sample rate: " << sampleRate);
 
    sfz::Sfizz synth;
    synth.setSamplesPerBlock(blockSize);
    synth.setSampleRate(sampleRate);
    synth.enableFreeWheeling();
    
    if (!synth.setOversamplingFactor(oversampling)) {
        LOG_ERROR("Bad oversampling factor: " << oversampling);
        std::exit(-1);
    }
    LOG_INFO("Oversampling factor: " << oversampling);

    
    if (!synth.loadSfzFile(sfzPath)) {
        LOG_ERROR("There was an error loading the SFZ file.");
        std::exit(-1);
    }
    LOG_INFO(synth.getNumRegions() << " regions in the SFZ.");

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

    if (useEOT) {
        LOG_INFO("-- Cutting the rendering at the last MIDI End of Track message");
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
    std::vector<float> leftBuffer (blockSize);
    std::vector<float> rightBuffer (blockSize);
    std::vector<float> interleavedBuffer (2 * blockSize);

    std::array outputBuffers { leftBuffer.data(), rightBuffer.data() };

    while (evIdx < midiFile.getNumEvents(trackIdx)) {
        const auto midiEvent = midiFile.getEvent(trackIdx, evIdx);
        const auto sampleIndex = static_cast<int>(midiEvent.seconds * sampleRateDouble);
        if (sampleIndex > nextBlockSentinel) {
            synth.renderBlock(outputBuffers.data(), blockSize);
            
            writeInterleaved(leftBuffer, rightBuffer, interleavedBuffer);

            numFramesWritten += outputFile.writef(interleavedBuffer.data(), blockSize);
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

    if (!useEOT) {
        auto averagePower = meanSquared(interleavedBuffer);
        while (averagePower > 1e-12f || nextBlockSentinel == blockSize) {
            synth.renderBlock(outputBuffers.data(), blockSize);
            writeInterleaved(leftBuffer, rightBuffer, interleavedBuffer);
            numFramesWritten += outputFile.writef(interleavedBuffer.data(), blockSize);
            blockStartTime = static_cast<double>(nextBlockSentinel) / sampleRateDouble; 
            nextBlockSentinel += blockSize;
            averagePower = meanSquared(interleavedBuffer);
        }
    }

    outputFile.writeSync();
    LOG_INFO("Wrote " << blockStartTime << " seconds of sound data in"
              << outputPath.string() << " (" << numFramesWritten << " frames)");

    return 0;
}