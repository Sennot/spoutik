#include "../src/core/AttemptGuard.hpp"
#include "../src/core/NoclipHitDetector.hpp"
#include "../src/core/TrainingPlan.hpp"
#include "../src/core/PercentageMath.hpp"
#include "../src/core/TrainingStats.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

using namespace baconsistent::core;

namespace {
bool close(double a, double b, double epsilon = 0.0001) {
    return std::abs(a - b) <= epsilon;
}
}

int main() {

    {
        // v0.4.2 validity rules. Noclip being enabled is not itself a fail:
        // only a suppressed lethal collision blocks the current A -> B run.
        AttemptGuard guard;
        guard.begin(false, true, true);
        assert(guard.countable());
        guard.observeSuppressedDeath();
        assert(!guard.countable());
        assert(guard.blockedByNoclip());

        guard.begin(false, true, false);
        guard.observeSuppressedDeath();
        assert(guard.countable()); // noclip protection toggle is OFF

        guard.begin(true, true, true);
        assert(!guard.countable());
        assert(guard.blockedByPractice());

        guard.begin(false, false, true);
        guard.observePracticeMode(true);
        assert(guard.countable()); // practice protection toggle is OFF

        guard.begin(false, true, true);
        guard.observePracticeMode(true);
        assert(!guard.countable());
        guard.observePracticeMode(false);
        assert(!guard.countable()); // sticky until restart
    }



    {
        // v0.4.2 regression: StartPos/reset bookkeeping must never become a
        // noclip hit merely because destroyPlayer returned with a live player.
        NoclipHitDetector detector;
        detector.reset();

        // Calls before the first gameplay frame are ignored entirely.
        assert(!detector.observe(0x111, true, true, false, false));
        detector.advanceFrame();

        // Null/no-object calls are service events, not collisions.
        assert(!detector.observe(0, true, true, false, false));

        // Death-Tracker-style fallback: first live object is only a baseline.
        assert(!detector.observe(0x111, true, true, false, false));
        assert(!detector.observe(0x111, true, true, false, false));
        assert(detector.observe(0x222, true, true, false, false));

        detector.reset();
        detector.advanceFrame();
        // If GD's own ignore-damage flags are active, the first real surviving
        // collision is enough to identify an actually-used noclip.
        assert(detector.observe(0x333, true, true, false, true));

        detector.reset();
        detector.advanceFrame();
        assert(!detector.observe(0x444, true, false, false, true)); // real death
        assert(!detector.observe(0x555, true, true, true, true));  // level end
    }

    {
        // Blitzkrieg-compatible percentage definitions: old 2.1 is pure X,
        // while 2.2 is based on travel time (and therefore speed portals).
        assert(close(legacy21PercentFromX(250.0, 1000.0), 25.0));
        assert(close(modern22PercentFromTime(3.0, 10.0), 30.0));
        assert(close(legacy21PercentFromX(-50.0, 1000.0), 0.0));
        assert(close(modern22PercentFromTime(12.0, 10.0), 100.0));
    }
    {
        // Dual-mode normalization must preserve physical indexes. A duplicate
        // marker in either scale is removed as a pair, not from one side only.
        auto dual = normalizeDualPercentMarkers(
            {70.0, 10.0, 30.0, 30.005, 90.0},
            {66.0, 8.0, 27.0, 27.004, 92.0}
        );
        assert(dual.legacy21.size() == dual.modern22.size());
        assert(dual.legacy21.size() == 6); // 0 + four unique markers + 100
        assert(close(dual.legacy21[1], 10.0));
        assert(close(dual.modern22[1], 8.0));
        assert(close(dual.legacy21[3], 70.0));
        assert(close(dual.modern22[3], 66.0));
    }

    {
        TrainingPlan plan(10, 20);
        assert(plan.size() == 10);
        assert(close(plan.segment(0).start, 0.0));
        assert(close(plan.segment(0).end, 10.0));
        assert(close(plan.segment(9).start, 90.0));
        assert(close(plan.segment(9).end, 100.0));
        assert(plan.recommendedBackwards() == 9);
    }

    {
        // Real StartPos profiles are not required to use equal boundaries.
        TrainingPlan plan(10, 20);
        plan.reconfigureBoundaries({0.0, 8.7, 17.3, 31.9, 72.4, 100.0}, 20);
        assert(plan.parts() == 5);
        assert(close(plan.segment(0).end, 8.7));
        assert(close(plan.segment(2).start, 17.3));
        assert(close(plan.segment(2).end, 31.9));
        assert(close(plan.segment(4).start, 72.4));
        assert(plan.recommendedBackwards() == 4);
    }

    {
        // Scanner input is normalized: implicit ends, sorting, duplicates,
        // out-of-range values and invalid floats are handled safely.
        auto boundaries = boundariesFromStartPosPercentages({50.0, 10.0, 10.005, -4.0, 105.0, std::numeric_limits<double>::quiet_NaN()});
        assert(boundaries.size() == 4);
        assert(close(boundaries[0], 0.0));
        assert(close(boundaries[1], 10.0));
        assert(close(boundaries[2], 50.0));
        assert(close(boundaries[3], 100.0));
    }

    {
        TrainingPlan plan(10, 20);
        TrainingSession session(&plan);
        session.setSelected(7); // 70 -> 80
        session.beginAttempt(70.2);
        assert(!session.update(79.9, true, 1.0).has_value());
        auto counted = session.update(80.0, true, 1.0);
        assert(counted.has_value() && counted.value() == 1);
        assert(!session.update(95.0, true, 1.0).has_value()); // only once / attempt
    }

    {
        // Irregular StartPos segment with a longer run: only the small fixed
        // part is counted and the run may continue freely.
        TrainingPlan plan(10, 1);
        plan.reconfigureBoundaries({0.0, 13.25, 28.6, 61.4, 100.0}, 1);
        TrainingSession session(&plan);
        session.setSelected(2); // 28.6 -> 61.4
        session.beginAttempt(28.55);
        auto counted = session.update(88.0, true, 0.2);
        assert(counted.has_value() && counted.value() == 1);
        assert(plan.count(2) == 1);
    }

    {
        TrainingPlan plan(10, 1);
        TrainingSession session(&plan);
        session.setSelected(7);
        session.beginAttempt(70.0);
        auto counted = session.update(95.0, true, 1.0);
        assert(counted.has_value() && counted.value() == 1);

        session.beginAttempt(70.0);
        assert(!session.update(95.0, true, 1.0).has_value());
        assert(plan.count(7) == 1);
    }

    {
        TrainingPlan plan(10, 20);
        TrainingSession session(&plan);
        session.setSelected(7);
        session.beginAttempt(0.0);
        assert(!session.update(90.0, true, 1.5).has_value());
        assert(plan.count(7) == 0);
    }

    {
        TrainingPlan plan(10, 2);
        TrainingSession session(&plan);
        session.setSelected(9);
        for (int i = 0; i < 2; ++i) {
            session.beginAttempt(90.0);
            auto counted = session.update(100.0, true, 1.0);
            assert(counted.has_value());
        }
        assert(plan.completed(9));
        assert(plan.previousIncomplete(9) == 8);
    }

    {
        TrainingPlan plan(10, 20);
        plan.decodeCounts("1,2,3,4,5,6,7,8,9,20");
        assert(plan.count(0) == 1);
        assert(plan.count(9) == 20);
        auto encoded = plan.encodeCounts();

        TrainingPlan roundTrip(10, 20);
        roundTrip.decodeCounts(encoded);
        assert(roundTrip.count(6) == 7);
        assert(roundTrip.count(9) == 20);
    }

    {
        TrainingPlan plan(4, 3);
        assert(close(plan.segment(0).end, 25.0));
        assert(close(plan.segment(3).start, 75.0));
        plan.decodeCounts("999,-3,broken,2");
        assert(plan.count(0) == 3);
        assert(plan.count(1) == 0);
        assert(plan.count(2) == 0);
        assert(plan.count(3) == 2);
    }

    {
        // Switching between 2.1 and 2.2 percentage labels is represented by
        // reconfiguring the same segment indexes; repetitions stay attached.
        TrainingPlan plan(10, 20);
        plan.reconfigureBoundaries({0, 10, 20, 30, 100}, 20); // pretend 2.1
        plan.increment(2);
        plan.increment(2);
        plan.reconfigureBoundaries({0, 8.5, 18.2, 29.7, 100}, 20); // pretend 2.2
        assert(plan.parts() == 4);
        assert(plan.count(2) == 2);
        assert(close(plan.segment(2).start, 18.2));
    }


    {
        // Per-stage targets are independent while the global default remains
        // useful for every other fixed segment.
        TrainingPlan plan(4, 20);
        plan.setTarget(3, 30);
        plan.setTarget(1, 10);
        assert(plan.target() == 20);
        assert(plan.target(0) == 20);
        assert(plan.target(1) == 10);
        assert(plan.target(3) == 30);
        assert(plan.totalGoal() == 80);
        assert(plan.remaining(3) == 30);

        auto encoded = plan.encodeTargets();
        TrainingPlan roundTrip(4, 20);
        roundTrip.decodeTargets(encoded);
        assert(roundTrip.target(0) == 20);
        assert(roundTrip.target(1) == 10);
        assert(roundTrip.target(3) == 30);

        // Persistence order used by TrainingManager must preserve counts above
        // the global default when a stage has a larger custom goal.
        for (int i = 0; i < 25; ++i) plan.increment(3);
        assert(plan.remaining(3) == 5);
        auto counts = plan.encodeCounts();
        TrainingPlan persisted(4, 20);
        persisted.decodeTargets(encoded);
        persisted.decodeCounts(counts);
        assert(persisted.count(3) == 25);
        assert(persisted.target(3) == 30);
    }

    {
        // Statistics mirror the useful Blitzkrieg-style concepts (attempts,
        // success rate, time and streaks) but are attached to fixed stages and
        // Baconsistent rounds.
        TrainingStats stats;
        stats.configureStages(3);

        stats.beginAttempt(2);
        stats.tick(1.0);
        stats.markSuccess(2);
        stats.finishAttempt();
        assert(stats.lifetime().attempts == 1);
        assert(stats.lifetime().successes == 1);
        assert(stats.lifetime().currentStreak == 1);
        assert(stats.stage(2).bestStreak == 1);

        stats.beginAttempt(2);
        stats.tick(0.5);
        stats.finishAttempt();
        assert(stats.lifetime().attempts == 2);
        assert(stats.lifetime().successes == 1);
        assert(stats.lifetime().currentStreak == 0);
        assert(close(stats.lifetime().successRate(), 50.0));

        stats.beginAttempt(1);
        stats.markSuccess(1);
        stats.finishAttempt();
        auto summary = stats.completeRound(1);
        assert(summary.round == 1);
        assert(summary.stats.attempts == 3);
        assert(stats.round().attempts == 0);
        assert(stats.lifetime().attempts == 3);
        assert(stats.history().size() == 1);

        auto encoded = stats.encode();
        TrainingStats restored;
        restored.configureStages(3);
        restored.decode(encoded);
        assert(restored.lifetime().attempts == 3);
        assert(restored.history().size() == 1);
        assert(restored.stage(2).attempts == 2);
        restored.resetCurrentRound();
        assert(restored.round().attempts == 0);
        assert(restored.lifetime().attempts == 3);
        assert(restored.history().size() == 1);
        assert(formatDuration(65.0) == "1m 05s");

        // Protected attempts disappear from stats completely rather than
        // lowering success rate or keeping their playtime.
        TrainingStats protectedStats;
        protectedStats.configureStages(1);
        protectedStats.beginAttempt(0);
        protectedStats.tick(1.0);
        protectedStats.cancelAttempt();
        assert(protectedStats.lifetime().attempts == 0);
        assert(close(protectedStats.lifetime().playtimeSeconds, 0.0));
        assert(protectedStats.stage(0).attempts == 0);
    }

    std::cout << "Baconsistent core tests passed\n";
    return 0;
}
