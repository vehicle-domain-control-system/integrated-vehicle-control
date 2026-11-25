#define main ExistingCoordinatorMain
#include "test_domain_request_coordinator.c"
#undef main

static int TestFinalResultBackpressure(unsigned invalidation)
{
    Fixture_t fixture;
    DomainIf_MobileRequest_t request = DoorRequest(71U, DOOR_TARGET_LOCK);
    DomainIf_BcmCommandResult_t result;
    size_t count;
    unsigned before;
    CHECK(FixtureInit(&fixture) == 0);
    CHECK(DomainRequestCoordinator_HandleMobileRequest(&fixture.coordinator, &request, 0U) ==
          DOMAIN_REQUEST_COORDINATOR_OK);
    result = DoorResult(&fixture, REQUEST_RESULT_DONE, RESULT_REASON_NONE); /* Host evidence only. */
    fixture.fake.publishResult = invalidation == 3U ? DOMAIN_IF_REJECTED : DOMAIN_IF_QUEUE_FULL;
    CHECK(DomainRequestCoordinator_HandleBcmResult(&fixture.coordinator, &result, 50U) ==
          DOMAIN_REQUEST_COORDINATOR_OK);
    before = fixture.fake.publishCount;
    fixture.fake.publishResult = DOMAIN_IF_OK;
    if (invalidation == 1U) fixture.fake.auth.sessionVerified = false;
    if (invalidation == 2U) ++fixture.fake.auth.currentSessionId;
    CHECK(DomainRequestCoordinator_Poll(&fixture.coordinator, 60U, &count) == DOMAIN_REQUEST_COORDINATOR_OK);
    CHECK(fixture.fake.submitCount == 1U); /* Never replay the ECU command. */
    if (invalidation == 0U) {
        CHECK(fixture.fake.publishCount == before + 1U);
        CHECK(fixture.fake.lastResult.resultInfo.result == REQUEST_RESULT_DONE);
        CHECK(fixture.fake.lastResult.commandId == result.commandId);
        CHECK(fixture.fake.lastResult.resultAgeMs == 10U);
    } else CHECK(fixture.fake.publishCount == before);
    before = fixture.fake.publishCount;
    CHECK(DomainRequestCoordinator_Poll(&fixture.coordinator, 70U, &count) == DOMAIN_REQUEST_COORDINATOR_OK);
    CHECK(fixture.fake.publishCount == before);
    return 0;
}

int main(void)
{
    CHECK(TestFinalResultBackpressure(0U) == 0);
    CHECK(TestFinalResultBackpressure(1U) == 0);
    CHECK(TestFinalResultBackpressure(2U) == 0);
    CHECK(TestFinalResultBackpressure(3U) == 0);
    CHECK(TestLockAndUnlock() == 0);
    CHECK(TestEcuResults() == 0);
    CHECK(TestTimeoutAndRollover() == 0);
    puts("door connection: 7/7 PASS (Host only; no production authorization)");
    return 0;
}
