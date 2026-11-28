// Copyright 2026 Gavin Sun
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Qualification lifecycle: receipts, fences, epochs, quarantine, and bounded
// retirement. A timeout, pending poll, failed poll call, or duplicate
// observation never retires an uncertain resource.

#include "check.h"

#include <array>
#include <cstdint>
#include <limits>

#include "qualification_fake.h"
#include "qualify/lifecycle.h"

namespace {

using lumen::Status;
using namespace lumen::qualification;
using namespace lumen::qualification::test;

LeaseSet reserve(std::uint64_t epoch, std::uint64_t resource_set_id) {
  const std::array<std::uint64_t, 2> ids = {11, 12};
  const auto result = reserve_leases(epoch, resource_set_id, ids);
  CHECK(result.ok());
  return result.ok() ? result.value() : LeaseSet{};
}

void reserve_validates_its_inputs() {
  const std::array<std::uint64_t, 2> ids = {1, 2};
  CHECK(!reserve_leases(0, 42, ids).ok());
  CHECK(!reserve_leases(7, 0, ids).ok());
  CHECK(!reserve_leases(7, 42, std::span<const std::uint64_t>()).ok());
  const std::array<std::uint64_t, 2> duplicate = {1, 1};
  CHECK(!reserve_leases(7, 42, duplicate).ok());

  const auto valid = reserve_leases(7, 42, ids);
  REQUIRE(valid.ok());
  CHECK(valid.value().state == LeaseState::reserved);
  CHECK(valid.value().epoch == 7);
  CHECK(valid.value().resource_set_id == 42);
  CHECK(valid.value().resource_ids.size() == 2);
  CHECK(!eligible_for_retirement(valid.value()));
}

void immediate_completion_retires_after_acceptance() {
  LeaseSet leases = reserve(7, 42);
  CHECK(request_destroy(leases, 11) == Status::invalid_input);

  const lumen_event_t event = event_token(0x1001);
  REQUIRE(observe_receipt(accepted_receipt(7, event, 42), leases) ==
          Status::ok);
  CHECK(leases.state == LeaseState::inflight);
  CHECK(leases.owns_fence);
  CHECK(leases.accepted_event_id == 0x1001u);
  CHECK(request_destroy(leases, 11) == Status::invalid_input);

  REQUIRE(observe_fence(complete_fence(42, 7, 0x1001), leases) == Status::ok);
  CHECK(leases.state == LeaseState::retired);
  CHECK(!leases.owns_fence);
  CHECK(eligible_for_retirement(leases));
  CHECK(request_destroy(leases, 11) == Status::ok);
  CHECK(request_destroy(leases, 12) == Status::ok);
  CHECK(request_destroy(leases, 13) == Status::invalid_input);
}

void delayed_completion_keeps_leases() {
  LeaseSet leases = reserve(7, 42);
  const lumen_event_t event = event_token(0x1002);
  REQUIRE(observe_receipt(accepted_receipt(7, event, 42), leases) ==
          Status::ok);

  CHECK(observe_fence(pending_fence(42, 7, 0x1002), leases) == Status::timeout);
  CHECK(leases.state == LeaseState::inflight);
  CHECK(request_destroy(leases, 11) == Status::invalid_input);

  REQUIRE(observe_fence(complete_fence(42, 7, 0x1002), leases) == Status::ok);
  CHECK(leases.state == LeaseState::retired);
}

void rejection_retires_without_side_effects() {
  LeaseSet leases = reserve(7, 42);
  REQUIRE(observe_receipt(rejected_receipt(LUMEN_STATUS_DEVICE_ERROR),
                          leases) == Status::ok);
  CHECK(leases.state == LeaseState::retired);
  CHECK(!leases.owns_fence);
  CHECK(leases.accepted_event_id == 0);
  CHECK(request_destroy(leases, 11) == Status::ok);

  // A successful call with a rejected disposition is contradictory: fail
  // closed and keep the resources quarantined.
  LeaseSet contradiction = reserve(7, 42);
  CHECK(observe_receipt(rejected_receipt(LUMEN_STATUS_OK), contradiction) ==
        Status::invalid_input);
  CHECK(contradiction.state == LeaseState::quarantined);
  CHECK(request_destroy(contradiction, 11) == Status::invalid_input);
}

void accepted_requires_an_owned_fence() {
  LeaseSet leases = reserve(7, 42);
  CHECK(observe_receipt(accepted_receipt(7, nullptr, 42), leases) ==
        Status::invalid_input);
  CHECK(leases.state == LeaseState::quarantined);
  CHECK(!leases.owns_fence);
  CHECK(request_destroy(leases, 11) == Status::invalid_input);

  LeaseSet error_status = reserve(7, 42);
  CHECK(observe_receipt(make_receipt(LUMEN_QUAL_ACCEPTED,
                                     LUMEN_STATUS_DEVICE_ERROR, 7,
                                     event_token(0x1003), 42),
                        error_status) == Status::invalid_input);
  CHECK(error_status.state == LeaseState::quarantined);
}

void uncertainty_retains_the_full_set_without_a_fence() {
  LeaseSet leases = reserve(7, 42);
  REQUIRE(observe_receipt(uncertain_receipt(7, nullptr, 42), leases) ==
          Status::ok);
  CHECK(leases.state == LeaseState::quarantined);
  CHECK(!leases.owns_fence);
  CHECK(request_destroy(leases, 11) == Status::invalid_input);
  CHECK(request_destroy(leases, 12) == Status::invalid_input);

  CHECK(observe_fence(pending_fence(42, 7, 0), leases) == Status::timeout);
  CHECK(leases.state == LeaseState::quarantined);
  CHECK(observe_fence(query_error_fence(42, 7, 0), leases) ==
        Status::device_error);
  CHECK(leases.state == LeaseState::quarantined);
  CHECK(request_destroy(leases, 11) == Status::invalid_input);

  REQUIRE(observe_fence(complete_fence(42, 7, 0), leases) == Status::ok);
  CHECK(leases.state == LeaseState::retired);
  CHECK(request_destroy(leases, 11) == Status::ok);
}

void uncertainty_with_a_fence_matches_event_identity() {
  LeaseSet leases = reserve(7, 42);
  REQUIRE(observe_receipt(uncertain_receipt(7, event_token(0x2002), 42),
                          leases) == Status::ok);
  CHECK(leases.state == LeaseState::quarantined);
  CHECK(leases.owns_fence);
  CHECK(leases.accepted_event_id == 0x2002u);

  CHECK(observe_fence(complete_fence(42, 7, 0x9999), leases) == Status::stale);
  CHECK(leases.state == LeaseState::quarantined);

  REQUIRE(observe_fence(complete_fence(42, 7, 0x2002), leases) == Status::ok);
  CHECK(leases.state == LeaseState::retired);
}

void timeout_then_late_success_retires() {
  LeaseSet leases = reserve(7, 42);
  const lumen_event_t event = event_token(0x3003);
  REQUIRE(observe_receipt(accepted_receipt(7, event, 42), leases) ==
          Status::ok);
  CHECK(observe_fence(pending_fence(42, 7, 0x3003), leases) == Status::timeout);
  CHECK(leases.state == LeaseState::inflight);
  CHECK(request_destroy(leases, 11) == Status::invalid_input);

  REQUIRE(observe_fence(complete_fence(42, 7, 0x3003), leases) == Status::ok);
  CHECK(leases.state == LeaseState::retired);
}

void failed_reset_does_not_retire() {
  LeaseSet leases = reserve(7, 42);
  REQUIRE(observe_receipt(uncertain_receipt(7, nullptr, 42), leases) ==
          Status::ok);

  CHECK(apply_recovery(recovery_proof(7, 8, false, true), leases) ==
        Status::device_error);
  CHECK(leases.state == LeaseState::quarantined);

  CHECK(apply_recovery(recovery_proof(7, 8, true, false), leases) ==
        Status::device_error);
  CHECK(leases.state == LeaseState::quarantined);

  CHECK(apply_recovery(
            recovery_proof(7, 8, true, true, LUMEN_STATUS_DEVICE_ERROR),
            leases) == Status::device_error);
  CHECK(leases.state == LeaseState::quarantined);

  CHECK(apply_recovery(recovery_proof(7, 8, true, true, LUMEN_STATUS_OK,
                                      LUMEN_QUAL_RECOVERY_NONE),
                       leases) == Status::unsupported);
  CHECK(leases.state == LeaseState::quarantined);

  CHECK(apply_recovery(recovery_proof(7, 8, true, true, LUMEN_STATUS_OK,
                                      LUMEN_QUAL_RECOVERY_CONTEXT, 0),
                       leases) == Status::invalid_input);
  CHECK(leases.state == LeaseState::quarantined);
  CHECK(request_destroy(leases, 11) == Status::invalid_input);

  REQUIRE(apply_recovery(recovery_proof(7, 8, true, true), leases) ==
          Status::ok);
  CHECK(leases.state == LeaseState::retired);
  CHECK(leases.epoch == 8);
  CHECK(!leases.owns_fence);
  CHECK(request_destroy(leases, 11) == Status::ok);
}

void stale_and_duplicate_observations_do_not_retire() {
  LeaseSet leases = reserve(8, 42);
  const lumen_event_t event = event_token(0x4004);
  REQUIRE(observe_receipt(accepted_receipt(8, event, 42), leases) ==
          Status::ok);
  CHECK(observe_receipt(accepted_receipt(8, event, 42), leases) ==
        Status::stale);
  CHECK(leases.state == LeaseState::inflight);

  CHECK(observe_fence(complete_fence(42, 7, 0x4004), leases) == Status::stale);
  CHECK(observe_fence(complete_fence(43, 8, 0x4004), leases) == Status::stale);
  CHECK(leases.state == LeaseState::inflight);

  CHECK(apply_recovery(recovery_proof(7, 8, true, true), leases) ==
        Status::stale);
  CHECK(leases.state == LeaseState::inflight);

  REQUIRE(observe_fence(complete_fence(42, 8, 0x4004), leases) == Status::ok);
  CHECK(observe_fence(complete_fence(42, 8, 0x4004), leases) == Status::stale);
  CHECK(apply_recovery(recovery_proof(8, 9, true, true), leases) ==
        Status::stale);
  CHECK(leases.state == LeaseState::retired);
}

void epoch_wrap_is_rejected() {
  LeaseSet leases = reserve((std::numeric_limits<std::uint64_t>::max)(), 42);
  REQUIRE(observe_receipt(
              uncertain_receipt((std::numeric_limits<std::uint64_t>::max)(),
                                nullptr, 42),
              leases) == Status::ok);

  CHECK(
      apply_recovery(recovery_proof((std::numeric_limits<std::uint64_t>::max)(),
                                    0, true, true),
                     leases) == Status::invalid_input);
  CHECK(leases.state == LeaseState::quarantined);

  CHECK(
      apply_recovery(recovery_proof((std::numeric_limits<std::uint64_t>::max)(),
                                    (std::numeric_limits<std::uint64_t>::max)(),
                                    true, true),
                     leases) == Status::invalid_input);
  CHECK(leases.state == LeaseState::quarantined);

  LeaseSet skipped = reserve(5, 42);
  REQUIRE(observe_receipt(uncertain_receipt(5, nullptr, 42), skipped) ==
          Status::ok);
  CHECK(apply_recovery(recovery_proof(5, 7, true, true), skipped) ==
        Status::invalid_input);
  CHECK(skipped.state == LeaseState::quarantined);
}

void early_destruction_is_rejected_until_retirement() {
  LeaseSet reserved = reserve(7, 42);
  CHECK(request_destroy(reserved, 11) == Status::invalid_input);

  LeaseSet inflight = reserve(7, 42);
  REQUIRE(observe_receipt(accepted_receipt(7, event_token(0x5005), 42),
                          inflight) == Status::ok);
  CHECK(request_destroy(inflight, 11) == Status::invalid_input);

  LeaseSet quarantined = reserve(7, 42);
  REQUIRE(observe_receipt(uncertain_receipt(7, nullptr, 42), quarantined) ==
          Status::ok);
  CHECK(request_destroy(quarantined, 11) == Status::invalid_input);

  LeaseSet retired = reserve(7, 42);
  REQUIRE(observe_receipt(rejected_receipt(LUMEN_STATUS_TIMEOUT), retired) ==
          Status::ok);
  CHECK(request_destroy(retired, 11) == Status::ok);
  CHECK(request_destroy(retired, 99) == Status::invalid_input);
}

void failed_event_needs_a_stop_guarantee() {
  LeaseSet leases = reserve(7, 42);
  const lumen_event_t event = event_token(0x6006);
  REQUIRE(observe_receipt(accepted_receipt(7, event, 42), leases) ==
          Status::ok);

  CHECK(observe_fence(failed_fence(42, 7, 0x6006, false), leases) ==
        Status::device_error);
  CHECK(leases.state == LeaseState::quarantined);
  CHECK(request_destroy(leases, 11) == Status::invalid_input);

  CHECK(observe_fence(failed_fence(42, 7, 0x6006, true), leases) ==
        Status::device_error);
  CHECK(leases.state == LeaseState::retired);
  CHECK(request_destroy(leases, 11) == Status::ok);
}

void failed_poll_call_is_not_a_fence() {
  LeaseSet leases = reserve(7, 42);
  const lumen_event_t event = event_token(0x7007);
  REQUIRE(observe_receipt(accepted_receipt(7, event, 42), leases) ==
          Status::ok);

  FenceObservation observation = query_error_fence(42, 7, 0x7007);
  CHECK(observe_fence(observation, leases) == Status::device_error);
  CHECK(leases.state == LeaseState::inflight);

  observation.query_status = LUMEN_STATUS_TIMEOUT;
  CHECK(observe_fence(observation, leases) == Status::timeout);
  CHECK(leases.state == LeaseState::inflight);
  CHECK(request_destroy(leases, 11) == Status::invalid_input);
}

void malformed_receipts_are_rejected_without_side_effects() {
  LeaseSet leases = reserve(7, 42);

  Receipt receipt = accepted_receipt(7, event_token(0x8008), 42);
  receipt.struct_size = 0;
  CHECK(observe_receipt(receipt, leases) == Status::invalid_input);
  CHECK(leases.state == LeaseState::reserved);

  receipt = accepted_receipt(7, event_token(0x8008), 42);
  receipt.abi_major = 99;
  CHECK(observe_receipt(receipt, leases) == Status::invalid_input);
  CHECK(leases.state == LeaseState::reserved);

  receipt = accepted_receipt(7, event_token(0x8008), 42);
  receipt.disposition = 99;
  CHECK(observe_receipt(receipt, leases) == Status::invalid_input);
  CHECK(leases.state == LeaseState::reserved);

  receipt = accepted_receipt(7, event_token(0x8008), 42);
  receipt.call_status = 99;
  CHECK(observe_receipt(receipt, leases) == Status::invalid_input);
  CHECK(leases.state == LeaseState::reserved);

  receipt = accepted_receipt(6, event_token(0x8008), 42);
  CHECK(observe_receipt(receipt, leases) == Status::stale);
  CHECK(leases.state == LeaseState::reserved);

  receipt = accepted_receipt(7, event_token(0x8008), 43);
  CHECK(observe_receipt(receipt, leases) == Status::stale);
  CHECK(leases.state == LeaseState::reserved);

  FenceObservation fence = complete_fence(42, 7, 0x8008);
  CHECK(observe_fence(fence, leases) == Status::invalid_input);
  CHECK(leases.state == LeaseState::reserved);
}

} // namespace

int main() {
  reserve_validates_its_inputs();
  immediate_completion_retires_after_acceptance();
  delayed_completion_keeps_leases();
  rejection_retires_without_side_effects();
  accepted_requires_an_owned_fence();
  uncertainty_retains_the_full_set_without_a_fence();
  uncertainty_with_a_fence_matches_event_identity();
  timeout_then_late_success_retires();
  failed_reset_does_not_retire();
  stale_and_duplicate_observations_do_not_retire();
  epoch_wrap_is_rejected();
  early_destruction_is_rejected_until_retirement();
  failed_event_needs_a_stop_guarantee();
  failed_poll_call_is_not_a_fence();
  malformed_receipts_are_rejected_without_side_effects();
  return lumen::test::summary("p0-qual-lifecycle");
}
