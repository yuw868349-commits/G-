#include <catch2/catch_test_macros.hpp>
#include "core/model_cascade.hpp"

using namespace swiftagent;

TEST_CASE("cascade routes chores to small tier by default") {
    ModelCascade cascade;
    CHECK(cascade.route_for(Role::Chore) == Tier::Small);
    CHECK(cascade.route_for(Role::Decision) == Tier::Large);
}

TEST_CASE("divergence increases small-tier divergence rate") {
    ModelCascade cascade;
    CHECK(cascade.divergence_rate(Tier::Small) == 0.0);
    cascade.record_outcome(Tier::Small, Role::Chore, true);
    cascade.record_outcome(Tier::Small, Role::Chore, false);
    CHECK(cascade.divergence_rate(Tier::Small) == 0.5);
}

TEST_CASE("governor pins chores to large tier after repeated failures") {
    ModelCascade cascade;
    cascade.set_max_failures_before_pin(2);
    cascade.record_failure(Tier::Small, Role::Chore);
    cascade.record_failure(Tier::Small, Role::Chore);
    CHECK(cascade.route_for(Role::Chore) == Tier::Large);
}

TEST_CASE("decision tier routes to large by default and is observable") {
    // The Decision role MUST be routed to the Large tier so that
    // model cascading exercises a real provider distinction, not a
    // no-op.  The orchestrator then observes the outcome and feeds
    // it back into the cascade's per-role statistics.
    ModelCascade cascade;
    CHECK(cascade.route_for(Role::Decision) == Tier::Large);
    cascade.record_outcome(Tier::Large, Role::Decision, true);
    cascade.record_outcome(Tier::Large, Role::Decision, false);
    CHECK(cascade.divergence_rate(Tier::Large) == 0.5);
    CHECK(cascade.divergence_rate(Tier::Large) == 0.5);
}
