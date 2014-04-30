//
//  MidiProcessor.cpp
//  OpenGLApp
//
//  Created by Eva Leonard on 25/03/2014.
//  Copyright (c) 2014 Eva Leonard. All rights reserved.
//

#include "MidiProcessor.h"

#include <exception>

using namespace jdksmidi;
using namespace std;
using namespace OpenGLApp;

MidiProcessor::MidiProcessor(string inputFilename) : inStream(MIDIFileReadStreamFile(inputFilename.c_str())), inFilename(inputFilename)
{
}

bool MidiProcessor::isValid()
{
    return this->inStream.IsValid();
}

OSStatus MidiProcessor::GetSynthFromGraph(AUGraph &inGraph, AudioUnit &outSynth)
{
    UInt32 numNodes;
    OSStatus res = noErr;
    FailIf((res = AUGraphGetNodeCount(inGraph, &numNodes)), fail, "AUGraphGetNodeCount");
    
    for (UInt32 i = 0; i < numNodes; ++i) {
        AUNode node;
        FailIf((res = AUGraphGetIndNode(inGraph, i, &node)), fail, "AUGraphGetIndNode");
        
        AudioComponentDescription desc;
        FailIf((res = AUGraphNodeInfo(inGraph, node, &desc, 0)), fail, "AUGraphNodeInfo");
        
        if (desc.componentType == kAudioUnitType_MusicDevice) {
            FailIf((res = AUGraphNodeInfo(inGraph, node, 0, &outSynth)), fail, "AUGraphNodeInfo");
            return noErr;
        }
    }
    
fail:
    return -1;
}

int MidiProcessor::getNumTracks()
{
    return convertedFilenames.size();
}

std::vector<std::string> MidiProcessor::getConvertedTrackNames()
{
    return convertedFilenames;
}

OSStatus MidiProcessor::SetUpGraph(AUGraph &inGraph, UInt32 numFrames, Float64 &sampleRate)
{
    OSStatus res = noErr;
    AudioUnit outputUnit = 0;
    AUNode outputNode;
    
    UInt32 nodeCount;
    FailIf((res = AUGraphGetNodeCount(inGraph, &nodeCount)), home, "AUGraphGetNodeCount");
    
    for (int i = 0; i < (int)nodeCount; ++i) {
        AUNode node;
        FailIf((res = AUGraphGetIndNode(inGraph, i, &node)), home, "AUGraphGetIndNode");
        
        AudioComponentDescription desc;
        AudioUnit unit;
        FailIf((res = AUGraphNodeInfo(inGraph, node, &desc, &unit)), home, "AUGraphNodeInfo");
        
        if (desc.componentType == kAudioUnitType_Output) {
            if (outputUnit == 0) {
                outputUnit = unit;
                FailIf((res = AUGraphNodeInfo(inGraph, node, 0, &outputUnit)), home, "AUGraphNodeInfo");
                
                FailIf((res = AUGraphRemoveNode(inGraph, node)), home, "AUGraphRemoveNode");
                
                desc.componentSubType = kAudioUnitSubType_GenericOutput;
                FailIf((res = AUGraphAddNode(inGraph, &desc, &node)), home, "AUGraphAddNode");
                FailIf((res = AUGraphNodeInfo(inGraph, node, NULL, &unit)), home, "AUGraphNodeInfo");
                
                outputUnit = unit;
                outputNode = node;
                
                FailIf((res = AudioUnitSetProperty(outputUnit,
                                                   kAudioUnitProperty_SampleRate,
                                                   kAudioUnitScope_Output, 0,
                                                   &sampleRate, sizeof(sampleRate))), home, "AudioUnitSetProperty: kAudioUnitProperty_SampleRate");
                
                i = -1;
            }
        }
        else
        {
            if (outputUnit) {
                if (desc.componentType != kAudioUnitType_MusicDevice) {
                    FailIf((res = AUGraphConnectNodeInput(inGraph, node, 0, outputNode, 0)), home, "AUGraphConnectNodeInput");
                }
                
                FailIf((res = AudioUnitSetProperty(unit,
                                                   kAudioUnitProperty_SampleRate,
                                                   kAudioUnitScope_Output, 0, &sampleRate, sizeof(sampleRate))), home, "AudioUnitSetProperty: kAudioUnitProperty_SampleRate");
            }
        }
        FailIf((res = AudioUnitSetProperty(unit, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, 0, &numFrames, sizeof(numFrames))), home, "AudioUnitSetProperty: kAudioUnitProperty_MaximumFramesPerSlice");
    }
    
home:
    return res;
}

OSStatus MidiProcessor::LoadMusicSequence(std::string filePath, MusicSequence &seq, MusicSequenceLoadFlags loadFlags)
{
    OSStatus res = noErr;
    CFURLRef url = NULL;
    
    FailIf((res = NewMusicSequence(&seq)), home, "NewMusicSequence");
    
    url = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault, (const UInt8*)filePath.c_str(), filePath.length(), false);
    
    FailIf((res = MusicSequenceFileLoad(seq, url, 0, loadFlags)), home, "MusicSequenceFileLoad");
    
home:
    if (url) CFRelease(url);
    return res;
}

void MidiProcessor::WriteConvertedOutputFile(std::string outputFilePath, OSType dataFormat, Float64 sampleRate, MusicTimeStamp sequenceLength, AUGraph inputGraph, UInt32 numFrames, MusicPlayer player)
{
    OSStatus res = 0;
    UInt32 size;
    
    CAStreamBasicDescription outputFormat;
    outputFormat.mChannelsPerFrame = 2;
    outputFormat.mSampleRate = sampleRate;
    outputFormat.mFormatID = dataFormat;
    
    AudioFileTypeID destFileType;
    CAAudioFileFormats::Instance()->InferFileFormatFromFilename(outputFilePath.c_str(), destFileType);
    
    outputFormat.mBytesPerPacket = outputFormat.mChannelsPerFrame * 2;
    outputFormat.mFramesPerPacket = 1;
    outputFormat.mBytesPerFrame = outputFormat.mBytesPerPacket;
    outputFormat.mBitsPerChannel = 16;
    
    if (destFileType == kAudioFileWAVEType) {
        outputFormat.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
    } else {
        outputFormat.mFormatFlags = kLinearPCMFormatFlagIsBigEndian
        | kLinearPCMFormatFlagIsSignedInteger
        | kLinearPCMFormatFlagIsPacked;
    }
    
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8*)outputFilePath.c_str(), outputFilePath.length(), false);
    
    ExtAudioFileRef outfile;
    res = ExtAudioFileCreateWithURL(url, destFileType, &outputFormat, NULL, kAudioFileFlags_EraseFile, &outfile);
    if (url) CFRelease(url);
    
    AudioUnit outputUnit = NULL;
    UInt32 nodeCount;
    FailIf((res = AUGraphGetNodeCount(inputGraph, &nodeCount)), fail, "AUGraphGetNodeCount");
    
    for (UInt32 i = 0; i < nodeCount; ++i) {
        AUNode node;
        FailIf((res = AUGraphGetIndNode(inputGraph, i, &node)), fail,
               "AUGraphGetIndNode");
        AudioComponentDescription desc;
        FailIf((res = AUGraphNodeInfo(inputGraph, node, &desc, NULL)), fail, "AUGraphNodeInfo");
        
        if (desc.componentType == kAudioUnitType_Output) {
            FailIf((res = AUGraphNodeInfo(inputGraph, node, 0, &outputUnit)), fail, "AUGraphNodeInfo");
            break;
        }
    }
    
    FailIf((res = (outputUnit == NULL)), fail, "outputUnit == NULL");
    {
        CAStreamBasicDescription clientFormat = CAStreamBasicDescription();
        size = sizeof(clientFormat);
        
        FailIf((res = AudioUnitGetProperty(outputUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, &clientFormat, &size)), fail, "AudioUnitGetProperty: kAudioUnitProperty_StreamFormat");
        size = sizeof(clientFormat);
        FailIf((res = ExtAudioFileSetProperty(outfile, kExtAudioFileProperty_ClientDataFormat, size, &clientFormat)), fail, "ExtAudioFileSetProperty: kExtAudioFileProperty_ClientDataFormat");
        {
            MusicTimeStamp currentTime;
            AUOutputBL outputBuffer (clientFormat, numFrames);
            AudioTimeStamp tStamp;
            memset(&tStamp, 0, sizeof(AudioTimeStamp));
            tStamp.mFlags = kAudioTimeStampSampleTimeValid;
            do {
                outputBuffer.Prepare();
                AudioUnitRenderActionFlags actionFlags = 0;
                FailIf((res = AudioUnitRender(outputUnit, &actionFlags, &tStamp, 0, numFrames, outputBuffer.ABL())), fail, "AudioUnitRender");
                
                tStamp.mSampleTime += numFrames;
                
                FailIf((res = ExtAudioFileWrite(outfile, numFrames, outputBuffer.ABL())), fail, "ExtAudioFileWrite");
                
                FailIf((res = MusicPlayerGetTime(player, &currentTime)), fail, "MusicPlayerGetTime");
            } while (currentTime < sequenceLength);
        }
    }
    
    ExtAudioFileDispose(outfile);
    
    return;
    
fail:
    printf("Problem: %ld\n", (long)res);
    exit(1);
}

void MidiProcessor::convertTrack(std::string filepath)
{
    OSStatus res;
    MusicSequence seq;
    
    Float32 maxCPULoad = .8;
    
    FailIf((res = LoadMusicSequence(filepath, seq, 0)), fail, "LoadMusicSequence");
    
    {
        AUGraph graph = 0;
        AudioUnit synth = 0;
        
        OSType dataFormat = 0;
        StrToOSType("lpcm", dataFormat);
        
        FailIf((res = MusicSequenceGetAUGraph(seq, &graph)), fail, "MusicSequenceGetAUGraph");
        
        FailIf((res = AUGraphOpen(graph)), fail, "AUGraphOpen");
        
        FailIf((res = GetSynthFromGraph(graph, synth)), fail, "GetSynthFromGraph");
        
        FailIf((res = AudioUnitSetProperty(synth, kAudioUnitProperty_CPULoad, kAudioUnitScope_Global, 0, &maxCPULoad, sizeof(maxCPULoad))), fail,
               "AudioUnitSetProperty: kAudioUnitProperty_CPULoad");
        {
            UInt32 val = 1;
        
            FailIf((res = AudioUnitSetProperty(synth, kAudioUnitProperty_OfflineRender, kAudioUnitScope_Global, 0, &val, sizeof(val))), fail, "AudioUnitySetProperty: kAudioUnitProperty_OfflineRender");
        }
        
        FailIf((res = SetUpGraph(graph, numFrames, sampleRate)), fail, "SetUpGraph");
        
        MusicPlayer player;
        FailIf((res = NewMusicPlayer(&player)), fail, "NewMusicPlayer");
        FailIf((res = MusicPlayerSetSequence(player, seq)), fail, "MusicPlayerSetSequence");
        
        MusicTimeStamp sequenceLength = 0;
        
        MusicTrack track;
        UInt32 propSize = sizeof(MusicTimeStamp);
        FailIf((res = MusicSequenceGetIndTrack(seq, 0, &track)), fail, "MusicSequenceGetIndTrack");
        FailIf((res = MusicTrackGetProperty(track, kSequenceTrackProperty_TrackLength,
                                            &sequenceLength, &propSize)), fail, "MusicTrackProperty: kSequenceTrackProperty_TrackLength");
        sequenceLength += 8;
        
        FailIf((res = MusicPlayerSetTime(player, 0)), fail, "MusicPlayerSetTime");
        FailIf((res = MusicPlayerPreroll(player)), fail, "MusicPlayerPreroll");
        
        //startRunningTime = CAHostTimeBase::GetTheCurrentTime();
        
        FailIf((res = MusicPlayerStart(player)), fail, "MusicPlayerStart");
        
        std::string outputFilePath = GetOutputFilePath(filepath);
        
        WriteConvertedOutputFile(outputFilePath, dataFormat, sampleRate, sequenceLength, graph, numFrames, player);
        
        FailIf((res = MusicPlayerStop(player)), fail, "MusicPlayerStop");
        
        FailIf((res = DisposeMusicPlayer(player)), fail, "DisposeMusicPlayer");
        FailIf((res = DisposeMusicSequence(seq)), fail, "DisposeMusicSequence");
        
        convertedFilenames.push_back(outputFilePath);
        
    }
    
    
    return;
    
    
fail:
    printf("Error = %ld\n", (long)res);
    exit(1);
}

void MidiProcessor::convertTracks()
{
    if (trackFilenames.size() == 0) {
        throw runtime_error("No track filenames on record. Did you forget to call splitTracks()?");
    }
    
    std::cout << "Starting conversion of tracks..." << std::endl;
    
    for(auto filepath : trackFilenames) {
        convertTrack(filepath);
    }
    
    std::cout << "Finished converting " << trackFilenames.size() << " track files to WAVs!" << std::endl;
}

void MidiProcessor::splitTracks()
{
    if (!this->isValid())
    {
        throw runtime_error("Input MIDI file not valid");
    }
    
    MIDIMultiTrack tracks(1);
    
    MIDIFileReadMultiTrack track_loader(&tracks);
    
    MIDIFileRead reader(&this->inStream, &track_loader);
    
    int num_tracks = reader.ReadNumTracks();
    tracks.ClearAndResize(num_tracks);
    
    if (!reader.Parse()) {
        throw runtime_error("Unable to parse input MIDI");
    }
    
    MIDITrack& firstTrack = *tracks.GetTrack(0);
    int tempoTrackIndex = 0;
    int numTempoEvents = firstTrack.GetNumEvents();
    
    for (int i = 1; i <= num_tracks; ++i)
    {
        MIDITrack& track = *tracks.GetTrack(i - 1);
        auto outFileName = this->getFilenameForTrack(i);
        MIDIFileWriteStreamFileName out_stream(outFileName.c_str());
        
        if (!out_stream.IsValid()) {
            throw runtime_error("Couldn't create output stream for file: " + outFileName);
        }
        
        MIDIFileWrite writer(&out_stream);
        
        auto numEvents = track.GetNumEvents();
        
        writer.WriteFileHeader(1, 1, tracks.GetClksPerBeat());
        
        if (!track.EventsOrderOK()) {
            throw runtime_error("Track events out of order");
        }
        
        writer.WriteTrackHeader(0);
        
        MIDIClockTime msgTime;
        
        bool wroteInitialNote = false;

        for (int j = 0; j < numEvents; ++j) {
            auto msg = track.GetEventAddress(j);
            if (msg->IsNoOp()) {
                continue;
            }
            
            msgTime = msg->GetTime();
            
            if (msg->IsDataEnd()) {
                break;
            }
            
            writer.WriteEvent(*msg);
            
            if (writer.ErrorOccurred()) {
                throw runtime_error("Error occurred while writing events");
            }
            
            if (i > 1) {
                while (tempoTrackIndex < numTempoEvents) {
                    auto curTempoEvent = firstTrack.GetEventAddress(tempoTrackIndex);
                    auto nextEventInCurTrack = track.GetEventAddress(j + 1);
                    if (curTempoEvent->GetTime() < nextEventInCurTrack->GetTime()) {
                        // Next event in track occurs after the next tempo event, this is where we need to write it.
                        writer.WriteEvent(*curTempoEvent);
                        tempoTrackIndex++;
                    } else {
                        break;
                    }
                }
            }
            
            if (!wroteInitialNote) {
                auto nextEventInCurTrack = track.GetEventAddress(j + 1);
                auto nextEventTime = nextEventInCurTrack->GetTime();
                if (nextEventTime > 0) {
                    // Write an initial 'silent' note so tracks don't begin playing immediately if their first actual notes appear later in the sequence.
                    MIDITimedBigMessage m;
                    m.SetTime(0);
                    m.SetNoteOn(1, 60, 1);
                    writer.WriteEvent(m);
                    m.SetNoteOff(1, 60, 127);
                    writer.WriteEvent(m);
                    wroteInitialNote = true;
                } else if (nextEventInCurTrack->IsNoteOn()) {
                    // If we come across a NoteOn event before having written our 'silent' note, then we don't need to write ours.
                    wroteInitialNote = true;
                }
            }
            
        }
        
        writer.WriteEndOfTrack(msgTime);
        
        writer.RewriteTrackLength();
        
        trackFilenames.push_back(outFileName);
        
        tempoTrackIndex = 0;
    }
    
    
};

string MidiProcessor::GetOutputFilePath(std::string filepath)
{
    auto lastDotPosition = filepath.find_last_of('.');
    ostringstream os;
    auto trackFilename = string(filepath, 0, lastDotPosition);
    os << trackFilename << ".wav";
    return os.str();
}

string MidiProcessor::getFilenameForTrack(int trackNum)
{
    auto lastDotPosition = this->inFilename.find_last_of('.');
    ostringstream os;
    auto trackFilename = string(this->inFilename, 0, lastDotPosition);
    os << trackFilename << "track" << trackNum << ".mid";
    return os.str();
}
