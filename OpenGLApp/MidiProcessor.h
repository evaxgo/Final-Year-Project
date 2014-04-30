//
//  MidiProcessor.h
//  OpenGLApp
//
//  Created by Eva Leonard on 25/03/2014.
//  Copyright (c) 2014 Eva Leonard. All rights reserved.
//

#ifndef __OpenGLApp__MidiProcessor__
#define __OpenGLApp__MidiProcessor__

#include <iostream>
#include <string>
#include <vector>

#include <jdksmidi/world.h>
#include <jdksmidi/midi.h>
#include <jdksmidi/msg.h>
#include <jdksmidi/sysex.h>
#include <jdksmidi/parser.h>
#include <jdksmidi/track.h>
#include <jdksmidi/multitrack.h>
#include <jdksmidi/fileread.h>
#include <jdksmidi/filereadmultitrack.h>
#include <jdksmidi/filewrite.h>

#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <CoreAudio/CoreAudioTypes.h>
#include <AudioToolbox/AudioToolbox.h>
#include <CoreMIDI/CoreMIDI.h>

#include "PublicUtility/AUOutputBL.h"
#include "PublicUtility/CAStreamBasicDescription.h"
#include "CAAudioFileFormats.h"

namespace OpenGLApp {
    
    class MidiProcessor
    {
    public:
        MidiProcessor(std::string inputFilename);
        
        bool isValid();
        void splitTracks();
        void convertTracks();
        int getNumTracks();
        std::vector<std::string> getConvertedTrackNames();
    private:
        const UInt32 numFrames = 512;
        Float64 sampleRate = 16000;
        
        std::string inFilename;
        std::vector<std::string> trackFilenames;
        std::vector<std::string> convertedFilenames;
        
        jdksmidi::MIDIFileReadStreamFile inStream;
        
        std::string getFilenameForTrack(int trackNum);
        std::string GetOutputFilePath(std::string filepath);
        void convertTrack(std::string filepath);
        void WriteConvertedOutputFile(std::string outputFilePath,
                                      OSType dataFormat,
                                      Float64 sampleRate,
                                      MusicTimeStamp sequenceLength,
                                      AUGraph inputGraph,
                                      UInt32 numFrames,
                                      MusicPlayer player);
        
        OSStatus LoadMusicSequence(std::string filePath, MusicSequence& seq, MusicSequenceLoadFlags loadFlags);
        
        OSStatus GetSynthFromGraph(AUGraph& inGraph, AudioUnit& outSynth);
        
        OSStatus SetUpGraph(AUGraph& inGraph, UInt32 numFrames, Float64& sampleRate);
        
    };
}



#endif /* defined(__OpenGLApp__MidiProcessor__) */
