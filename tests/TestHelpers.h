// ═══════════════════════════════════════════════════════════════════════════
//  TestHelpers.h — Common test helpers for MixCoach unit tests
//
//  Provides:
//    • TEST() macro (pass/fail counting)
//    • setupTrackAudioResult() — push simple peak/RMS data
//    • setupTrackAudioResultFull() — push full data (crest, correlation)
//    • getMessageText() — extract nth message from SharedData
//    • countMessagesOfType() — count messages by type
//
//  Usage in test .cpp files:
//    #include "TestHelpers.h"
//
//  ⚠️ SharedData must be heap-allocated (~65MB). Always use std::make_unique.
// ═══════════════════════════════════════════════════════════════════════════

#pragma once

#include <cstdio>
#include <cmath>

#include <juce_core/juce_core.h>

#include "Common/types/Types.h"
#include "Common/memory/SharedData.h"

// ─── Global test counters ─────────────────────────────────────────────────
static int gTestsPassed = 0;
static int gTestsFailed = 0;

// ═══════════════════════════════════════════════════════════════════════════
//  TEST macro — Counts pass/fail with file+line on failure
//
//  Usage:
//    TEST("My test name", value == expected);
// ═══════════════════════════════════════════════════════════════════════════
#define TEST(name, expr) do {                                                  \
    if (!(expr)) {                                                             \
        std::fprintf(stderr, "  \xe2\x9d\x8c FAIL: %s (%s:%d)\n",              \
                     name, __FILE__, __LINE__);                                \
        std::fflush(stderr);                                                   \
        gTestsFailed++;                                                        \
    } else {                                                                   \
        std::printf("  \xe2\x9c\x85 PASS: %s\n", name);                        \
        std::fflush(stdout);                                                   \
        gTestsPassed++;                                                        \
    }                                                                          \
} while(0)

// ═══════════════════════════════════════════════════════════════════════════
//  setupTrackAudioResult — Push simple peak/RMS telemetry to SharedData
//
//  Simulates the Messenger background worker pushing audio analysis results.
//  Sets peakLeft = peakRight = peakDb, rmsLeft = rmsRight = rmsDb.
//  Timestamp is set to current millis * 1000.
// ═══════════════════════════════════════════════════════════════════════════
static void setupTrackAudioResult(mixcoach::SharedData& sd, int slotIndex,
                                   float peakDb, float rmsDb)
{
    mixcoach::TrackAudioResult result;
    result.peakLeft  = peakDb;
    result.peakRight = peakDb;
    result.rmsLeft   = rmsDb;
    result.rmsRight  = rmsDb;
    result.timestampUs = static_cast<int64_t>(juce::Time::getMillisecondCounter()) * 1000;
    sd.updateTrackAudioResult(slotIndex, result);
}

// ═══════════════════════════════════════════════════════════════════════════
//  setupTrackAudioResultFull — Push full telemetry with crest + correlation
//
//  Like setupTrackAudioResult() but with separate L/R peaks, correlation,
//  and a uniform crestFactor applied to all 6 crestPerBand entries.
// ═══════════════════════════════════════════════════════════════════════════
static void setupTrackAudioResultFull(mixcoach::SharedData& sd, int slotIndex,
                                       float peakLeftDb, float peakRightDb,
                                       float rmsLeftDb, float rmsRightDb,
                                       float correlation, float crestFactor)
{
    mixcoach::TrackAudioResult result;
    result.peakLeft    = peakLeftDb;
    result.peakRight   = peakRightDb;
    result.rmsLeft     = rmsLeftDb;
    result.rmsRight    = rmsRightDb;
    result.correlation = correlation;
    for (int b = 0; b < 6; ++b)
        result.crestPerBand[b] = crestFactor;
    result.timestampUs = static_cast<int64_t>(juce::Time::getMillisecondCounter()) * 1000;
    sd.updateTrackAudioResult(slotIndex, result);
}

// ═══════════════════════════════════════════════════════════════════════════
//  getMessageText — Extract the text of the nth message from SharedData
//
//  @param sd     SharedData reference
//  @param index  Message index (0-based)
//  @return juce::String with the message text
// ═══════════════════════════════════════════════════════════════════════════
static juce::String getMessageText(mixcoach::SharedData& sd, int index)
{
    auto msg = sd.getMessage(index);
    return juce::String(msg.text);
}

// ═══════════════════════════════════════════════════════════════════════════
//  countMessagesOfType — Count messages in SharedData by MentorMessage::Type
//
//  @param sd    SharedData reference
//  @param type  The message type to count (Info, Tip, Warning, Achievement, etc.)
//  @return Count of messages with matching type
// ═══════════════════════════════════════════════════════════════════════════
static int countMessagesOfType(mixcoach::SharedData& sd, mixcoach::MentorMessage::Type type)
{
    int count = 0;
    int n = sd.getMessageCount();
    for (int i = 0; i < n; ++i)
        if (sd.getMessage(i).type == type)
            count++;
    return count;
}
