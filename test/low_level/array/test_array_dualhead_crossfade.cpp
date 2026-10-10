//
// Added to verify dual-head playback adopts secondary rate after crossfade
//

#include <boost/test/unit_test.hpp>
#include <cmath>
#include "ResamplingArrayFixture.h"
#include "utils.h"

extern unsigned char kick_raw[];
extern unsigned int kick_raw_len;

BOOST_AUTO_TEST_SUITE(test_array_dualhead_crossfade)

BOOST_FIXTURE_TEST_CASE(DualHeadCrossfadeAdoptsSecondaryRate, ResamplingArrayFixture) {
    const uint32_t numSamples = kick_raw_len / 2;

    resamplingArrayReader->begin();
    resamplingArrayReader->setUseDualPlaybackHead(true);
    resamplingArrayReader->setLoopType(loop_type::looptype_granular);
    resamplingArrayReader->setCrossfadeDurationInSamples(32);
    resamplingArrayReader->setPlaybackRate(1.0f);
    resamplingArrayReader->playRaw((int16_t *)kick_raw, numSamples, 1);
    resamplingArrayReader->setPlaybackRateForReadHead(2, 0.5f);

    int16_t actual[128];
    int16_t *buffers[1] = { actual };

    bool sawCrossfade = false;
    bool adoptedRate = false;
    const double expectedRate = 0.5;

    for (int iteration = 0; iteration < 200 && !adoptedRate; ++iteration) {
        auto read = resamplingArrayReader->read((void **)buffers, 128);
        if (read <= 0)
            break;

        double crossfadeValue = resamplingArrayReader->getCrossfade();
        if (crossfadeValue > 0.0 && crossfadeValue < 1.0)
            sawCrossfade = true;

        if (std::fabs(resamplingArrayReader->playbackRate() - expectedRate) < 1e-6)
            adoptedRate = true;
    }

    BOOST_CHECK_MESSAGE(sawCrossfade, "Crossfade never engaged for dual-head playback");
    BOOST_CHECK_MESSAGE(adoptedRate, "Primary head did not adopt the secondary playback rate after crossfade");
}

BOOST_FIXTURE_TEST_CASE(SetNextGrainHonoursCrossfadeState, ResamplingArrayFixture) {
    const uint32_t numSamples = kick_raw_len / 2;

    resamplingArrayReader->begin();
    resamplingArrayReader->setUseDualPlaybackHead(true);
    resamplingArrayReader->setLoopType(loop_type::looptype_granular);
    resamplingArrayReader->setCrossfadeDurationInSamples(64);
    resamplingArrayReader->setPlaybackRate(1.0f);
    resamplingArrayReader->playRaw((int16_t *)kick_raw, numSamples, 1);

    const uint32_t firstStart = 50;
    const uint32_t firstFinish = 250;
    const double firstRate = 0.4;
    const uint32_t initialFinish = resamplingArrayReader->getLoopFinish();
    bool applied = resamplingArrayReader->setNextGrain(firstStart, firstFinish, firstRate);
    BOOST_CHECK(applied);
    BOOST_CHECK_EQUAL(resamplingArrayReader->getLoopStart(), firstStart);
    BOOST_CHECK_EQUAL(resamplingArrayReader->getLoopFinish(), initialFinish);
    BOOST_CHECK_CLOSE(resamplingArrayReader->playbackRateForReadHead(2), firstRate, 0.001);

    int16_t actual[64];
    int16_t *buffers[1] = { actual };
    bool sawCrossfade = false;
    bool completedCrossfade = false;
    bool rejectedDuringCrossfade = false;
    const uint32_t secondStart = 80;
    const uint32_t secondFinish = 320;
    const double secondRate = 0.9;
    for (int iteration = 0; iteration < 600 && !completedCrossfade; ++iteration) {
        auto read = resamplingArrayReader->read((void **)buffers, 64);
        if (read <= 0)
            break;
        double crossfadeValue = resamplingArrayReader->getCrossfade();
        if (crossfadeValue > 0.0 && crossfadeValue < 1.0)
        {
            sawCrossfade = true;
            if (!rejectedDuringCrossfade)
            {
                bool rejected = resamplingArrayReader->setNextGrain(secondStart, secondFinish, secondRate);
                BOOST_CHECK_EQUAL(rejected, false);
                BOOST_CHECK_EQUAL(resamplingArrayReader->getLoopStart(), firstStart);
                BOOST_CHECK_EQUAL(resamplingArrayReader->getLoopFinish(), initialFinish);
                BOOST_CHECK_CLOSE(resamplingArrayReader->playbackRateForReadHead(2), firstRate, 0.001);
                rejectedDuringCrossfade = true;
            }
        }
        if (sawCrossfade && crossfadeValue == 0.0 && std::fabs(resamplingArrayReader->playbackRate() - firstRate) < 1e-6)
            completedCrossfade = true;
    }
    BOOST_REQUIRE_MESSAGE(sawCrossfade, "Failed to enter crossfade state for granular loop");
    BOOST_REQUIRE_MESSAGE(rejectedDuringCrossfade, "Never attempted setNextGrain() during crossfade");
    if (completedCrossfade)
        BOOST_CHECK_EQUAL(resamplingArrayReader->getLoopFinish(), firstFinish);
    else
        BOOST_WARN_MESSAGE(false, "Crossfade did not complete within the iteration window");
}

BOOST_AUTO_TEST_SUITE_END()
