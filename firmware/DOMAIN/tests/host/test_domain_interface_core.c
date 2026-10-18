#include <stdio.h>
#include <string.h>

#include "Domain_Interface_v0.5_C2_target.h"
#include "domain_interface_core_internal.h"

static unsigned int g_failedChecks;
static unsigned int g_executedTests;

#define CHECK(condition)                                                        \
    do                                                                          \
    {                                                                           \
        if (!(condition))                                                       \
        {                                                                       \
            (void)printf("CHECK failed at %s:%d: %s\n", __FILE__, __LINE__,   \
                         #condition);                                           \
            ++g_failedChecks;                                                   \
        }                                                                       \
    } while (0)

#define RUN_TEST(testFunction)                                                  \
    do                                                                          \
    {                                                                           \
        unsigned int failuresBefore = g_failedChecks;                           \
        ++g_executedTests;                                                      \
        testFunction();                                                         \
        (void)printf("%s: %s\n", #testFunction,                               \
                     (failuresBefore == g_failedChecks) ? "PASS" : "FAIL");   \
    } while (0)

static DomainIf_Status_t ObserverMobileA(
    const DomainIf_MobileRequest_t *request)
{
    (void)request;
    return DOMAIN_IF_OK;
}

static DomainIf_Status_t ObserverMobileB(
    const DomainIf_MobileRequest_t *request)
{
    (void)request;
    return DOMAIN_IF_REJECTED;
}

static DomainIf_Status_t ObserverWarningA(const DomainIf_WarningAck_t *ack)
{
    (void)ack;
    return DOMAIN_IF_OK;
}

static DomainIf_Status_t TxDoorA(const DomainIf_BcmDoorCommand_t *command)
{
    (void)command;
    return DOMAIN_IF_OK;
}

static DomainIf_Status_t TxDoorB(const DomainIf_BcmDoorCommand_t *command)
{
    (void)command;
    return DOMAIN_IF_NOT_AVAILABLE;
}

static DomainIf_Status_t TxWarningA(const DomainIf_WarningInfo_t *warning)
{
    (void)warning;
    return DOMAIN_IF_OK;
}

typedef enum
{
    TX_INDEX_BCM_DOOR = 0,
    TX_INDEX_BCM_CLIMATE,
    TX_INDEX_BCM_LIGHT,
    TX_INDEX_CIS_PERMISSION,
    TX_INDEX_WINDOW_COMMAND,
    TX_INDEX_WINDOW_PERMISSION,
    TX_INDEX_WINDOW_ALIVE,
    TX_INDEX_VSS_EVENT,
    TX_INDEX_VSS_WARNING,
    TX_INDEX_MOBILE_RESULT,
    TX_INDEX_MOBILE_BCM_DOOR,
    TX_INDEX_MOBILE_BCM_CLIMATE,
    TX_INDEX_MOBILE_BCM_LIGHT,
    TX_INDEX_MOBILE_BCM_STATUS,
    TX_INDEX_MOBILE_CIS_ENVIRONMENT,
    TX_INDEX_MOBILE_CIS_OCCUPANT,
    TX_INDEX_MOBILE_CIS_REAR,
    TX_INDEX_MOBILE_CIS_STATUS,
    TX_INDEX_MOBILE_VSS_STATUS,
    TX_INDEX_MOBILE_WINDOW_STATE,
    TX_INDEX_MOBILE_WINDOW_FAULT,
    TX_INDEX_MOBILE_WARNING,
    TX_INDEX_MOBILE_AVAILABILITY,
    TX_INDEX_MOBILE_DIGITAL_KEY_STATE,
    TX_INDEX_MOBILE_DIGITAL_KEY_RESULT,
    TX_INDEX_MOBILE_SETTINGS,
    TX_INDEX_COUNT
} TxIndex_t;

static unsigned int g_txCalls[TX_INDEX_COUNT];
static const void *g_txArguments[TX_INDEX_COUNT];
static DomainIf_Status_t g_txReturns[TX_INDEX_COUNT];

#define DEFINE_TX_CALLBACK(functionName_, index_, type_)                       \
    static DomainIf_Status_t functionName_(const type_ *value)                 \
    {                                                                          \
        ++g_txCalls[index_];                                                    \
        g_txArguments[index_] = (const void *)value;                            \
        return g_txReturns[index_];                                             \
    }

DEFINE_TX_CALLBACK(TxAllBcmDoor, TX_INDEX_BCM_DOOR,
                   DomainIf_BcmDoorCommand_t)
DEFINE_TX_CALLBACK(TxAllBcmClimate, TX_INDEX_BCM_CLIMATE,
                   DomainIf_BcmClimateCommand_t)
DEFINE_TX_CALLBACK(TxAllBcmLight, TX_INDEX_BCM_LIGHT,
                   DomainIf_BcmInteriorLightCommand_t)
DEFINE_TX_CALLBACK(TxAllCisPermission, TX_INDEX_CIS_PERMISSION,
                   DomainIf_CisPowerPermission_t)
DEFINE_TX_CALLBACK(TxAllWindowCommand, TX_INDEX_WINDOW_COMMAND,
                   DomainIf_WindowCommand_t)
DEFINE_TX_CALLBACK(TxAllWindowPermission, TX_INDEX_WINDOW_PERMISSION,
                   DomainIf_WindowOperationPermission_t)
DEFINE_TX_CALLBACK(TxAllWindowAlive, TX_INDEX_WINDOW_ALIVE,
                   DomainIf_WindowDomainAlive_t)
DEFINE_TX_CALLBACK(TxAllVssEvent, TX_INDEX_VSS_EVENT,
                   DomainIf_VssOneShotEvent_t)
DEFINE_TX_CALLBACK(TxAllVssWarning, TX_INDEX_VSS_WARNING,
                   DomainIf_VssWarningState_t)
DEFINE_TX_CALLBACK(TxAllMobileResult, TX_INDEX_MOBILE_RESULT,
                   DomainIf_MobileRequestResult_t)
DEFINE_TX_CALLBACK(TxAllMobileBcmDoor, TX_INDEX_MOBILE_BCM_DOOR, DoorState_t)
DEFINE_TX_CALLBACK(TxAllMobileBcmClimate, TX_INDEX_MOBILE_BCM_CLIMATE,
                   ClimateState_t)
DEFINE_TX_CALLBACK(TxAllMobileBcmLight, TX_INDEX_MOBILE_BCM_LIGHT,
                   InteriorLightState_t)
DEFINE_TX_CALLBACK(TxAllMobileBcmStatus, TX_INDEX_MOBILE_BCM_STATUS,
                   DomainIf_BcmStatusReport_t)
DEFINE_TX_CALLBACK(TxAllMobileCisEnvironment,
                   TX_INDEX_MOBILE_CIS_ENVIRONMENT,
                   CisCabinEnvironmentState_t)
DEFINE_TX_CALLBACK(TxAllMobileCisOccupant, TX_INDEX_MOBILE_CIS_OCCUPANT,
                   CisOccupantState_t)
DEFINE_TX_CALLBACK(TxAllMobileCisRear, TX_INDEX_MOBILE_CIS_REAR,
                   CisRearState_t)
DEFINE_TX_CALLBACK(TxAllMobileCisStatus, TX_INDEX_MOBILE_CIS_STATUS,
                   DomainIf_CisStatusReport_t)
DEFINE_TX_CALLBACK(TxAllMobileVssStatus, TX_INDEX_MOBILE_VSS_STATUS,
                   DomainIf_VssStateReport_t)
DEFINE_TX_CALLBACK(TxAllMobileWindowState, TX_INDEX_MOBILE_WINDOW_STATE,
                   WindowState_t)
DEFINE_TX_CALLBACK(TxAllMobileWindowFault, TX_INDEX_MOBILE_WINDOW_FAULT,
                   DomainIf_WindowFaultState_t)
DEFINE_TX_CALLBACK(TxAllMobileWarning, TX_INDEX_MOBILE_WARNING,
                   DomainIf_WarningInfo_t)
DEFINE_TX_CALLBACK(TxAllMobileAvailability, TX_INDEX_MOBILE_AVAILABILITY,
                   DomainIf_FunctionAvailabilitySet_t)
DEFINE_TX_CALLBACK(TxAllMobileDigitalKeyState,
                   TX_INDEX_MOBILE_DIGITAL_KEY_STATE, DigitalKeyState_t)
DEFINE_TX_CALLBACK(TxAllMobileDigitalKeyResult,
                   TX_INDEX_MOBILE_DIGITAL_KEY_RESULT,
                   DomainIf_DigitalKeyResult_t)
DEFINE_TX_CALLBACK(TxAllMobileSettings, TX_INDEX_MOBILE_SETTINGS,
                   DomainIf_UserSettingsState_t)

#undef DEFINE_TX_CALLBACK

typedef struct
{
    DomainIf_BcmDoorCommand_t doorCommand;
    DomainIf_BcmClimateCommand_t climateCommand;
    DomainIf_BcmInteriorLightCommand_t lightCommand;
    DomainIf_CisPowerPermission_t cisPermission;
    DomainIf_WindowCommand_t windowCommand;
    DomainIf_WindowOperationPermission_t windowPermission;
    DomainIf_WindowDomainAlive_t windowAlive;
    DomainIf_VssOneShotEvent_t vssEvent;
    DomainIf_VssWarningState_t vssWarning;
    DomainIf_MobileRequestResult_t mobileResult;
    DoorState_t doorState;
    ClimateState_t climateState;
    InteriorLightState_t lightState;
    DomainIf_BcmStatusReport_t bcmStatus;
    CisCabinEnvironmentState_t cisEnvironment;
    CisOccupantState_t cisOccupant;
    CisRearState_t cisRear;
    DomainIf_CisStatusReport_t cisStatus;
    DomainIf_VssStateReport_t vssStatus;
    WindowState_t windowState;
    DomainIf_WindowFaultState_t windowFault;
    DomainIf_WarningInfo_t warning;
    DomainIf_FunctionAvailabilitySet_t availability;
    DigitalKeyState_t digitalKeyState;
    DomainIf_DigitalKeyResult_t digitalKeyResult;
    DomainIf_UserSettingsState_t settings;
} TxFixtures_t;

static DomainIf_CommandContext_t MakeCommandContext(void)
{
    DomainIf_CommandContext_t context;

    (void)memset(&context, 0, sizeof(context));
    context.commandId = 10U;
    context.decisionSequence = 20U;
    context.hasRequestContext = true;
    context.requestContext.sessionId = 30U;
    context.requestContext.requestId = 40U;
    context.requestDeviceContextId = 50U;
    context.origin = COMMAND_ORIGIN_MOBILE_REQUEST;
    context.meta.quality = DATA_QUALITY_OK;
    return context;
}

static void InitTxFixtures(TxFixtures_t *fixture)
{
    static const DomainFunctionId_t functionIds[DOMAIN_FUNCTION_COUNT] =
    {
        DOMAIN_FUNCTION_DOOR,
        DOMAIN_FUNCTION_CLIMATE,
        DOMAIN_FUNCTION_INTERIOR_LIGHT,
        DOMAIN_FUNCTION_DIGITAL_KEY,
        DOMAIN_FUNCTION_WINDOW,
        DOMAIN_FUNCTION_REAR_WARNING,
        DOMAIN_FUNCTION_OCCUPANT_WARNING,
        DOMAIN_FUNCTION_VSS,
        DOMAIN_FUNCTION_ENVIRONMENT
    };
    size_t index;

    (void)memset(fixture, 0, sizeof(*fixture));
    fixture->doorCommand.context = MakeCommandContext();
    fixture->climateCommand.context = MakeCommandContext();
    fixture->lightCommand.context = MakeCommandContext();
    fixture->windowCommand.context = MakeCommandContext();
    fixture->cisPermission.quality = DATA_QUALITY_OK;
    fixture->cisPermission.meta.quality = DATA_QUALITY_OK;
    fixture->windowPermission.quality = DATA_QUALITY_OK;
    fixture->windowPermission.meta.quality = DATA_QUALITY_OK;
    fixture->windowAlive.meta.quality = DATA_QUALITY_OK;
    fixture->vssEvent.meta.quality = DATA_QUALITY_OK;
    fixture->mobileResult.deviceContextId = 1U;
    fixture->mobileResult.resultInfo.context.sessionId = 2U;
    fixture->mobileResult.resultInfo.context.requestId = 3U;
    fixture->mobileResult.hasVehicleResult = true;
    fixture->mobileResult.resultInfo.result = REQUEST_RESULT_ACCEPTED;
    fixture->mobileResult.resultInfo.reason = RESULT_REASON_NONE;
    fixture->mobileResult.resultInfo.confirmation =
        RESULT_CONFIRMATION_UNCONFIRMED;
    fixture->mobileResult.target = MOBILE_RESULT_TARGET_BCM;
    fixture->mobileResult.commandId = 4U;
    fixture->mobileResult.origin = COMMAND_ORIGIN_MOBILE_REQUEST;
    fixture->cisEnvironment.temperatureQuality.validity =
        VALUE_VALIDITY_INVALID;
    fixture->cisEnvironment.humidityQuality.validity = VALUE_VALIDITY_INVALID;
    fixture->cisEnvironment.illuminanceQuality.validity =
        VALUE_VALIDITY_INVALID;
    fixture->cisOccupant.presenceQuality.validity = VALUE_VALIDITY_INVALID;
    fixture->cisOccupant.countQuality.validity = VALUE_VALIDITY_INVALID;
    fixture->cisRear.distanceQuality.validity = VALUE_VALIDITY_INVALID;
    for (index = 0U; index < (size_t)DOMAIN_FUNCTION_COUNT; ++index)
    {
        fixture->availability.items[index].functionId = functionIds[index];
        fixture->availability.items[index].availability =
            FUNCTION_AVAILABILITY_AVAILABLE;
        fixture->availability.items[index].reason = RESULT_REASON_NONE;
    }
}

static DomainIf_TxPort_t MakeCompleteTxPort(void)
{
    DomainIf_TxPort_t port;

    (void)memset(&port, 0, sizeof(port));
    port.txBcmDoorCommand = TxAllBcmDoor;
    port.txBcmClimateCommand = TxAllBcmClimate;
    port.txBcmInteriorLightCommand = TxAllBcmLight;
    port.txCisPowerPermission = TxAllCisPermission;
    port.txWindowCommand = TxAllWindowCommand;
    port.txWindowOperationPermission = TxAllWindowPermission;
    port.txWindowDomainAlive = TxAllWindowAlive;
    port.txVssOneShotEvent = TxAllVssEvent;
    port.txVssWarningState = TxAllVssWarning;
    port.txMobileRequestResult = TxAllMobileResult;
    port.txMobileBcmDoorState = TxAllMobileBcmDoor;
    port.txMobileBcmClimateState = TxAllMobileBcmClimate;
    port.txMobileBcmInteriorLightState = TxAllMobileBcmLight;
    port.txMobileBcmStatus = TxAllMobileBcmStatus;
    port.txMobileCisEnvironment = TxAllMobileCisEnvironment;
    port.txMobileCisOccupant = TxAllMobileCisOccupant;
    port.txMobileCisRear = TxAllMobileCisRear;
    port.txMobileCisStatus = TxAllMobileCisStatus;
    port.txMobileVssStatus = TxAllMobileVssStatus;
    port.txMobileWindowState = TxAllMobileWindowState;
    port.txMobileWindowFault = TxAllMobileWindowFault;
    port.txMobileWarning = TxAllMobileWarning;
    port.txMobileFunctionAvailability = TxAllMobileAvailability;
    port.txMobileDigitalKeyState = TxAllMobileDigitalKeyState;
    port.txMobileDigitalKeyResult = TxAllMobileDigitalKeyResult;
    port.txMobileUserSettings = TxAllMobileSettings;
    return port;
}

static void ResetTxObservations(void)
{
    size_t index;

    (void)memset(g_txCalls, 0, sizeof(g_txCalls));
    (void)memset(g_txArguments, 0, sizeof(g_txArguments));
    for (index = 0U; index < (size_t)TX_INDEX_COUNT; ++index)
    {
        g_txReturns[index] = (DomainIf_Status_t)(index % 7U);
    }
}

static DomainIf_RxObserver_t MakeObserverA(void)
{
    DomainIf_RxObserver_t observer;

    (void)memset(&observer, 0, sizeof(observer));
    observer.onMobileRequest = ObserverMobileA;
    observer.onWarningAck = ObserverWarningA;
    return observer;
}

static DomainIf_RxObserver_t MakeObserverB(void)
{
    DomainIf_RxObserver_t observer;

    (void)memset(&observer, 0, sizeof(observer));
    observer.onMobileRequest = ObserverMobileB;
    return observer;
}

static DomainIf_TxPort_t MakeTxPortA(void)
{
    DomainIf_TxPort_t port;

    (void)memset(&port, 0, sizeof(port));
    port.txBcmDoorCommand = TxDoorA;
    port.txMobileWarning = TxWarningA;
    return port;
}

static DomainIf_TxPort_t MakeTxPortB(void)
{
    DomainIf_TxPort_t port;

    (void)memset(&port, 0, sizeof(port));
    port.txBcmDoorCommand = TxDoorB;
    return port;
}

static void Test_InitialState(void)
{
    CHECK(!DomainIf_IsInitialized());
    CHECK(!DomainIfCore_TestHasRxObserver());
    CHECK(!DomainIfCore_TestHasTxPort());
}

static void Test_PreInitRegistrationRejected(void)
{
    DomainIf_RxObserver_t observer = MakeObserverA();
    DomainIf_TxPort_t port = MakeTxPortA();

    CHECK(DomainIf_SetRxObserver(&observer) == DOMAIN_IF_NOT_INITIALIZED);
    CHECK(DomainIf_SetRxObserver(NULL) == DOMAIN_IF_NOT_INITIALIZED);
    CHECK(DomainIf_SetTxPort(&port) == DOMAIN_IF_NOT_INITIALIZED);
    CHECK(DomainIf_SetTxPort(NULL) == DOMAIN_IF_NOT_INITIALIZED);
    CHECK(!DomainIfCore_TestHasRxObserver());
    CHECK(!DomainIfCore_TestHasTxPort());
}

static void Test_PreInitTxRejected(void)
{
    TxFixtures_t fixture;

    InitTxFixtures(&fixture);
    CHECK(DomainIf_TxBcmDoorCommand(NULL) == DOMAIN_IF_INVALID_ARGUMENT);
    CHECK(DomainIf_TxBcmDoorCommand(&fixture.doorCommand) ==
          DOMAIN_IF_NOT_INITIALIZED);
}

static void Test_FirstInitialization(void)
{
    CHECK(DomainIf_Init() == DOMAIN_IF_OK);
    CHECK(DomainIf_IsInitialized());
    CHECK(!DomainIfCore_TestHasRxObserver());
    CHECK(!DomainIfCore_TestHasTxPort());
}

static void Test_ObserverNullRegistrationAndUnbind(void)
{
    DomainIf_RxObserver_t observer = MakeObserverA();
    DomainIf_RxObserver_t snapshot;

    CHECK(DomainIf_SetRxObserver(NULL) == DOMAIN_IF_OK);
    CHECK(!DomainIfCore_TestHasRxObserver());
    CHECK(DomainIf_SetRxObserver(&observer) == DOMAIN_IF_OK);
    CHECK(DomainIfCore_TestHasRxObserver());
    CHECK(DomainIf_SetRxObserver(NULL) == DOMAIN_IF_OK);
    CHECK(!DomainIfCore_TestHasRxObserver());
    (void)memset(&snapshot, 0xA5, sizeof(snapshot));
    DomainIfCore_TestCopyRxObserver(&snapshot);
    CHECK(snapshot.onMobileRequest == NULL);
    CHECK(snapshot.onWarningAck == NULL);
}

static void Test_TxPortNullRegistrationAndUnbind(void)
{
    DomainIf_TxPort_t port = MakeTxPortA();
    DomainIf_TxPort_t snapshot;

    CHECK(DomainIf_SetTxPort(NULL) == DOMAIN_IF_OK);
    CHECK(!DomainIfCore_TestHasTxPort());
    CHECK(DomainIf_SetTxPort(&port) == DOMAIN_IF_OK);
    CHECK(DomainIfCore_TestHasTxPort());
    CHECK(DomainIf_SetTxPort(NULL) == DOMAIN_IF_OK);
    CHECK(!DomainIfCore_TestHasTxPort());
    (void)memset(&snapshot, 0xA5, sizeof(snapshot));
    DomainIfCore_TestCopyTxPort(&snapshot);
    CHECK(snapshot.txBcmDoorCommand == NULL);
    CHECK(snapshot.txMobileWarning == NULL);
}

static void Test_ObserverIsCopiedByValue(void)
{
    DomainIf_RxObserver_t observer = MakeObserverA();
    DomainIf_RxObserver_t snapshot;

    CHECK(DomainIf_SetRxObserver(&observer) == DOMAIN_IF_OK);
    observer.onMobileRequest = ObserverMobileB;
    observer.onWarningAck = NULL;
    DomainIfCore_TestCopyRxObserver(&snapshot);
    CHECK(snapshot.onMobileRequest == ObserverMobileA);
    CHECK(snapshot.onWarningAck == ObserverWarningA);
}

static void Test_TxPortIsCopiedByValue(void)
{
    DomainIf_TxPort_t port = MakeTxPortA();
    DomainIf_TxPort_t snapshot;

    CHECK(DomainIf_SetTxPort(&port) == DOMAIN_IF_OK);
    port.txBcmDoorCommand = TxDoorB;
    port.txMobileWarning = NULL;
    DomainIfCore_TestCopyTxPort(&snapshot);
    CHECK(snapshot.txBcmDoorCommand == TxDoorA);
    CHECK(snapshot.txMobileWarning == TxWarningA);
}

static void Test_SerializedReregistrationReplacesTables(void)
{
    DomainIf_RxObserver_t observer = MakeObserverB();
    DomainIf_TxPort_t port = MakeTxPortB();
    DomainIf_RxObserver_t observerSnapshot;
    DomainIf_TxPort_t portSnapshot;

    CHECK(DomainIf_SetRxObserver(&observer) == DOMAIN_IF_OK);
    CHECK(DomainIf_SetTxPort(&port) == DOMAIN_IF_OK);
    DomainIfCore_TestCopyRxObserver(&observerSnapshot);
    DomainIfCore_TestCopyTxPort(&portSnapshot);
    CHECK(observerSnapshot.onMobileRequest == ObserverMobileB);
    CHECK(observerSnapshot.onWarningAck == NULL);
    CHECK(portSnapshot.txBcmDoorCommand == TxDoorB);
    CHECK(portSnapshot.txMobileWarning == NULL);
}

static void Test_DuplicateInitPreservesBindings(void)
{
    DomainIf_RxObserver_t observerBefore;
    DomainIf_RxObserver_t observerAfter;
    DomainIf_TxPort_t portBefore;
    DomainIf_TxPort_t portAfter;

    DomainIfCore_TestCopyRxObserver(&observerBefore);
    DomainIfCore_TestCopyTxPort(&portBefore);
    CHECK(DomainIf_Init() == DOMAIN_IF_OK);
    CHECK(DomainIf_IsInitialized());
    CHECK(DomainIfCore_TestHasRxObserver());
    CHECK(DomainIfCore_TestHasTxPort());
    DomainIfCore_TestCopyRxObserver(&observerAfter);
    DomainIfCore_TestCopyTxPort(&portAfter);
    CHECK(memcmp(&observerBefore, &observerAfter, sizeof(observerBefore)) == 0);
    CHECK(memcmp(&portBefore, &portAfter, sizeof(portBefore)) == 0);
}

static void Test_ObserverAndTxPortAreIndependent(void)
{
    DomainIf_RxObserver_t observer = MakeObserverA();
    DomainIf_TxPort_t port = MakeTxPortA();
    DomainIf_RxObserver_t observerSnapshot;
    DomainIf_TxPort_t portSnapshot;

    CHECK(DomainIf_SetRxObserver(&observer) == DOMAIN_IF_OK);
    CHECK(DomainIf_SetTxPort(&port) == DOMAIN_IF_OK);
    CHECK(DomainIf_SetRxObserver(NULL) == DOMAIN_IF_OK);
    CHECK(!DomainIfCore_TestHasRxObserver());
    CHECK(DomainIfCore_TestHasTxPort());
    DomainIfCore_TestCopyTxPort(&portSnapshot);
    CHECK(portSnapshot.txBcmDoorCommand == TxDoorA);

    CHECK(DomainIf_SetRxObserver(&observer) == DOMAIN_IF_OK);
    CHECK(DomainIf_SetTxPort(NULL) == DOMAIN_IF_OK);
    CHECK(DomainIfCore_TestHasRxObserver());
    CHECK(!DomainIfCore_TestHasTxPort());
    DomainIfCore_TestCopyRxObserver(&observerSnapshot);
    CHECK(observerSnapshot.onMobileRequest == ObserverMobileA);
}

static void Test_AllTxNullInputsRejected(void)
{
    CHECK(DomainIf_TxBcmDoorCommand(NULL) == DOMAIN_IF_INVALID_ARGUMENT);
    CHECK(DomainIf_TxBcmClimateCommand(NULL) == DOMAIN_IF_INVALID_ARGUMENT);
    CHECK(DomainIf_TxBcmInteriorLightCommand(NULL) ==
          DOMAIN_IF_INVALID_ARGUMENT);
    CHECK(DomainIf_TxCisPowerPermission(NULL) == DOMAIN_IF_INVALID_ARGUMENT);
    CHECK(DomainIf_TxWindowCommand(NULL) == DOMAIN_IF_INVALID_ARGUMENT);
    CHECK(DomainIf_TxWindowOperationPermission(NULL) ==
          DOMAIN_IF_INVALID_ARGUMENT);
    CHECK(DomainIf_TxWindowDomainAlive(NULL) == DOMAIN_IF_INVALID_ARGUMENT);
    CHECK(DomainIf_TxVssOneShotEvent(NULL) == DOMAIN_IF_INVALID_ARGUMENT);
    CHECK(DomainIf_TxVssWarningState(NULL) == DOMAIN_IF_INVALID_ARGUMENT);
    CHECK(DomainIf_TxMobileRequestResult(NULL) == DOMAIN_IF_INVALID_ARGUMENT);
    CHECK(DomainIf_TxMobileBcmDoorState(NULL) == DOMAIN_IF_INVALID_ARGUMENT);
    CHECK(DomainIf_TxMobileBcmClimateState(NULL) == DOMAIN_IF_INVALID_ARGUMENT);
    CHECK(DomainIf_TxMobileBcmInteriorLightState(NULL) ==
          DOMAIN_IF_INVALID_ARGUMENT);
    CHECK(DomainIf_TxMobileBcmStatus(NULL) == DOMAIN_IF_INVALID_ARGUMENT);
    CHECK(DomainIf_TxMobileCisEnvironment(NULL) == DOMAIN_IF_INVALID_ARGUMENT);
    CHECK(DomainIf_TxMobileCisOccupant(NULL) == DOMAIN_IF_INVALID_ARGUMENT);
    CHECK(DomainIf_TxMobileCisRear(NULL) == DOMAIN_IF_INVALID_ARGUMENT);
    CHECK(DomainIf_TxMobileCisStatus(NULL) == DOMAIN_IF_INVALID_ARGUMENT);
    CHECK(DomainIf_TxMobileVssStatus(NULL) == DOMAIN_IF_INVALID_ARGUMENT);
    CHECK(DomainIf_TxMobileWindowState(NULL) == DOMAIN_IF_INVALID_ARGUMENT);
    CHECK(DomainIf_TxMobileWindowFault(NULL) == DOMAIN_IF_INVALID_ARGUMENT);
    CHECK(DomainIf_TxMobileWarning(NULL) == DOMAIN_IF_INVALID_ARGUMENT);
    CHECK(DomainIf_TxMobileFunctionAvailability(NULL) ==
          DOMAIN_IF_INVALID_ARGUMENT);
    CHECK(DomainIf_TxMobileDigitalKeyState(NULL) ==
          DOMAIN_IF_INVALID_ARGUMENT);
    CHECK(DomainIf_TxMobileDigitalKeyResult(NULL) ==
          DOMAIN_IF_INVALID_ARGUMENT);
    CHECK(DomainIf_TxMobileUserSettings(NULL) == DOMAIN_IF_INVALID_ARGUMENT);
}

static void Test_TxPortAndCallbackAvailability(void)
{
    TxFixtures_t fixture;
    DomainIf_TxPort_t emptyPort;

    InitTxFixtures(&fixture);
    CHECK(DomainIf_SetTxPort(NULL) == DOMAIN_IF_OK);
    CHECK(DomainIf_TxBcmDoorCommand(&fixture.doorCommand) ==
          DOMAIN_IF_NOT_AVAILABLE);
    (void)memset(&emptyPort, 0, sizeof(emptyPort));
    CHECK(DomainIf_SetTxPort(&emptyPort) == DOMAIN_IF_OK);
    CHECK(DomainIf_TxBcmDoorCommand(&fixture.doorCommand) ==
          DOMAIN_IF_NOT_AVAILABLE);
}

#define CHECK_FORWARD(index_, call_, address_)                                 \
    do                                                                         \
    {                                                                          \
        CHECK((call_) == g_txReturns[index_]);                                 \
        CHECK(g_txCalls[index_] == 1U);                                        \
        CHECK(g_txArguments[index_] == (const void *)(address_));              \
    } while (0)

static void Test_AllTxWrappersForwardAndPreserveStatus(void)
{
    TxFixtures_t fixture;
    DomainIf_TxPort_t port = MakeCompleteTxPort();

    InitTxFixtures(&fixture);
    ResetTxObservations();
    CHECK(DomainIf_SetTxPort(&port) == DOMAIN_IF_OK);

    CHECK_FORWARD(TX_INDEX_BCM_DOOR,
                  DomainIf_TxBcmDoorCommand(&fixture.doorCommand),
                  &fixture.doorCommand);
    CHECK_FORWARD(TX_INDEX_BCM_CLIMATE,
                  DomainIf_TxBcmClimateCommand(&fixture.climateCommand),
                  &fixture.climateCommand);
    CHECK_FORWARD(TX_INDEX_BCM_LIGHT,
                  DomainIf_TxBcmInteriorLightCommand(&fixture.lightCommand),
                  &fixture.lightCommand);
    CHECK_FORWARD(TX_INDEX_CIS_PERMISSION,
                  DomainIf_TxCisPowerPermission(&fixture.cisPermission),
                  &fixture.cisPermission);
    CHECK_FORWARD(TX_INDEX_VSS_EVENT,
                  DomainIf_TxVssOneShotEvent(&fixture.vssEvent),
                  &fixture.vssEvent);
    CHECK_FORWARD(TX_INDEX_VSS_WARNING,
                  DomainIf_TxVssWarningState(&fixture.vssWarning),
                  &fixture.vssWarning);
    CHECK_FORWARD(TX_INDEX_MOBILE_RESULT,
                  DomainIf_TxMobileRequestResult(&fixture.mobileResult),
                  &fixture.mobileResult);
    CHECK_FORWARD(TX_INDEX_MOBILE_BCM_DOOR,
                  DomainIf_TxMobileBcmDoorState(&fixture.doorState),
                  &fixture.doorState);
    CHECK_FORWARD(TX_INDEX_MOBILE_BCM_CLIMATE,
                  DomainIf_TxMobileBcmClimateState(&fixture.climateState),
                  &fixture.climateState);
    CHECK_FORWARD(TX_INDEX_MOBILE_BCM_LIGHT,
                  DomainIf_TxMobileBcmInteriorLightState(&fixture.lightState),
                  &fixture.lightState);
    CHECK_FORWARD(TX_INDEX_MOBILE_BCM_STATUS,
                  DomainIf_TxMobileBcmStatus(&fixture.bcmStatus),
                  &fixture.bcmStatus);
    CHECK_FORWARD(TX_INDEX_MOBILE_CIS_ENVIRONMENT,
                  DomainIf_TxMobileCisEnvironment(&fixture.cisEnvironment),
                  &fixture.cisEnvironment);
    CHECK_FORWARD(TX_INDEX_MOBILE_CIS_OCCUPANT,
                  DomainIf_TxMobileCisOccupant(&fixture.cisOccupant),
                  &fixture.cisOccupant);
    CHECK_FORWARD(TX_INDEX_MOBILE_CIS_REAR,
                  DomainIf_TxMobileCisRear(&fixture.cisRear),
                  &fixture.cisRear);
    CHECK_FORWARD(TX_INDEX_MOBILE_CIS_STATUS,
                  DomainIf_TxMobileCisStatus(&fixture.cisStatus),
                  &fixture.cisStatus);
    CHECK_FORWARD(TX_INDEX_MOBILE_VSS_STATUS,
                  DomainIf_TxMobileVssStatus(&fixture.vssStatus),
                  &fixture.vssStatus);
    CHECK_FORWARD(TX_INDEX_MOBILE_WARNING,
                  DomainIf_TxMobileWarning(&fixture.warning),
                  &fixture.warning);
    CHECK_FORWARD(TX_INDEX_MOBILE_AVAILABILITY,
                  DomainIf_TxMobileFunctionAvailability(&fixture.availability),
                  &fixture.availability);
    CHECK_FORWARD(TX_INDEX_MOBILE_DIGITAL_KEY_STATE,
                  DomainIf_TxMobileDigitalKeyState(&fixture.digitalKeyState),
                  &fixture.digitalKeyState);
    CHECK_FORWARD(TX_INDEX_MOBILE_DIGITAL_KEY_RESULT,
                  DomainIf_TxMobileDigitalKeyResult(&fixture.digitalKeyResult),
                  &fixture.digitalKeyResult);
    CHECK_FORWARD(TX_INDEX_MOBILE_SETTINGS,
                  DomainIf_TxMobileUserSettings(&fixture.settings),
                  &fixture.settings);

    CHECK(DomainIf_TxWindowCommand(&fixture.windowCommand) ==
          DOMAIN_IF_UNSUPPORTED);
    CHECK(DomainIf_TxWindowOperationPermission(&fixture.windowPermission) ==
          DOMAIN_IF_UNSUPPORTED);
    CHECK(DomainIf_TxWindowDomainAlive(&fixture.windowAlive) ==
          DOMAIN_IF_UNSUPPORTED);
    CHECK(DomainIf_TxMobileWindowState(&fixture.windowState) ==
          DOMAIN_IF_UNSUPPORTED);
    CHECK(DomainIf_TxMobileWindowFault(&fixture.windowFault) ==
          DOMAIN_IF_UNSUPPORTED);
    CHECK(g_txCalls[TX_INDEX_WINDOW_COMMAND] == 0U);
    CHECK(g_txCalls[TX_INDEX_WINDOW_PERMISSION] == 0U);
    CHECK(g_txCalls[TX_INDEX_WINDOW_ALIVE] == 0U);
    CHECK(g_txCalls[TX_INDEX_MOBILE_WINDOW_STATE] == 0U);
    CHECK(g_txCalls[TX_INDEX_MOBILE_WINDOW_FAULT] == 0U);
}

#undef CHECK_FORWARD

static void Test_TxPortCopyAndReregistration(void)
{
    TxFixtures_t fixture;
    DomainIf_TxPort_t port = MakeCompleteTxPort();
    DomainIf_TxPort_t replacement = MakeTxPortB();

    InitTxFixtures(&fixture);
    ResetTxObservations();
    g_txReturns[TX_INDEX_BCM_DOOR] = DOMAIN_IF_QUEUE_FULL;
    CHECK(DomainIf_SetTxPort(&port) == DOMAIN_IF_OK);
    (void)memset(&port, 0, sizeof(port));
    CHECK(DomainIf_TxBcmDoorCommand(&fixture.doorCommand) ==
          DOMAIN_IF_QUEUE_FULL);
    CHECK(g_txCalls[TX_INDEX_BCM_DOOR] == 1U);

    CHECK(DomainIf_SetTxPort(&replacement) == DOMAIN_IF_OK);
    CHECK(DomainIf_TxBcmDoorCommand(&fixture.doorCommand) ==
          DOMAIN_IF_NOT_AVAILABLE);
}

static void Test_CommandValidationAndRequestContext(void)
{
    TxFixtures_t fixture;
    DomainIf_TxPort_t port = MakeCompleteTxPort();

    InitTxFixtures(&fixture);
    ResetTxObservations();
    CHECK(DomainIf_SetTxPort(&port) == DOMAIN_IF_OK);

    fixture.doorCommand.context.commandId = 0U;
    CHECK(DomainIf_TxBcmDoorCommand(&fixture.doorCommand) ==
          DOMAIN_IF_INVALID_ARGUMENT);
    fixture.doorCommand.context = MakeCommandContext();
    fixture.doorCommand.context.requestContext.requestId = 0U;
    CHECK(DomainIf_TxBcmDoorCommand(&fixture.doorCommand) ==
          DOMAIN_IF_INVALID_ARGUMENT);
    fixture.doorCommand.context = MakeCommandContext();
    fixture.doorCommand.context.hasRequestContext = false;
    CHECK(DomainIf_TxBcmDoorCommand(&fixture.doorCommand) ==
          DOMAIN_IF_INVALID_ARGUMENT);
    fixture.doorCommand.context = MakeCommandContext();
    fixture.doorCommand.context.origin = (CommandOrigin_t)99;
    CHECK(DomainIf_TxBcmDoorCommand(&fixture.doorCommand) ==
          DOMAIN_IF_INVALID_ARGUMENT);
    fixture.doorCommand.context = MakeCommandContext();
    fixture.doorCommand.target = (DoorLockTarget_t)99;
    CHECK(DomainIf_TxBcmDoorCommand(&fixture.doorCommand) ==
          DOMAIN_IF_INVALID_ARGUMENT);

    fixture.climateCommand.fanTarget = FAN_LEVEL_UNKNOWN;
    CHECK(DomainIf_TxBcmClimateCommand(&fixture.climateCommand) ==
          DOMAIN_IF_INVALID_ARGUMENT);
    fixture.climateCommand.fanTarget = FAN_LEVEL_OFF;
    fixture.climateCommand.thermalTargetLevelPercent = 101U;
    CHECK(DomainIf_TxBcmClimateCommand(&fixture.climateCommand) ==
          DOMAIN_IF_INVALID_ARGUMENT);
    fixture.lightCommand.levelPercent = 101U;
    CHECK(DomainIf_TxBcmInteriorLightCommand(&fixture.lightCommand) ==
          DOMAIN_IF_INVALID_ARGUMENT);
    CHECK(g_txCalls[TX_INDEX_BCM_DOOR] == 0U);
    CHECK(g_txCalls[TX_INDEX_BCM_CLIMATE] == 0U);
    CHECK(g_txCalls[TX_INDEX_BCM_LIGHT] == 0U);
}

static void Test_StateAndResultValidation(void)
{
    TxFixtures_t fixture;
    DomainIf_TxPort_t port = MakeCompleteTxPort();

    InitTxFixtures(&fixture);
    ResetTxObservations();
    CHECK(DomainIf_SetTxPort(&port) == DOMAIN_IF_OK);

    fixture.cisPermission.meta.quality = DATA_QUALITY_STALE;
    CHECK(DomainIf_TxCisPowerPermission(&fixture.cisPermission) ==
          DOMAIN_IF_INVALID_ARGUMENT);
    fixture.vssEvent.hasRelatedCommand = true;
    CHECK(DomainIf_TxVssOneShotEvent(&fixture.vssEvent) ==
          DOMAIN_IF_INVALID_ARGUMENT);
    fixture.mobileResult.target = MOBILE_RESULT_TARGET_DOMAIN_SETTING;
    CHECK(DomainIf_TxMobileRequestResult(&fixture.mobileResult) ==
          DOMAIN_IF_INVALID_ARGUMENT);
    fixture.mobileResult.target = MOBILE_RESULT_TARGET_BCM;
    fixture.mobileResult.hasVehicleResult = false;
    fixture.mobileResult.resultInfo.confirmation = RESULT_CONFIRMATION_CONFIRMED;
    CHECK(DomainIf_TxMobileRequestResult(&fixture.mobileResult) ==
          DOMAIN_IF_INVALID_ARGUMENT);
    fixture.doorState.lockState = (DoorLockState_t)99;
    CHECK(DomainIf_TxMobileBcmDoorState(&fixture.doorState) ==
          DOMAIN_IF_INVALID_ARGUMENT);
    fixture.climateState.thermalOutputLevelPercent = 101U;
    CHECK(DomainIf_TxMobileBcmClimateState(&fixture.climateState) ==
          DOMAIN_IF_INVALID_ARGUMENT);
    fixture.lightState.levelPercent = 101U;
    CHECK(DomainIf_TxMobileBcmInteriorLightState(&fixture.lightState) ==
          DOMAIN_IF_INVALID_ARGUMENT);
    fixture.bcmStatus.ecuState = (CommonEcuState_t)99;
    CHECK(DomainIf_TxMobileBcmStatus(&fixture.bcmStatus) ==
          DOMAIN_IF_INVALID_ARGUMENT);
    fixture.cisEnvironment.humidityQuality.validity = VALUE_VALIDITY_VALID;
    fixture.cisEnvironment.cabinHumidity = 10001U;
    CHECK(DomainIf_TxMobileCisEnvironment(&fixture.cisEnvironment) ==
          DOMAIN_IF_INVALID_ARGUMENT);
    fixture.cisOccupant.countQuality.validity = VALUE_VALIDITY_VALID;
    fixture.cisOccupant.occupantCount = 6U;
    CHECK(DomainIf_TxMobileCisOccupant(&fixture.cisOccupant) ==
          DOMAIN_IF_INVALID_ARGUMENT);
    fixture.cisRear.measurementState = (RearMeasurementState_t)99;
    CHECK(DomainIf_TxMobileCisRear(&fixture.cisRear) ==
          DOMAIN_IF_INVALID_ARGUMENT);
    fixture.cisStatus.piUartStatus = (DomainIf_CisInterfaceStatus_t)1;
    CHECK(DomainIf_TxMobileCisStatus(&fixture.cisStatus) ==
          DOMAIN_IF_INVALID_ARGUMENT);
    fixture.vssStatus.service.state = (VssState_t)99;
    CHECK(DomainIf_TxMobileVssStatus(&fixture.vssStatus) ==
          DOMAIN_IF_INVALID_ARGUMENT);
    fixture.warning.type = DOMAIN_WARNING_COUNT;
    CHECK(DomainIf_TxMobileWarning(&fixture.warning) ==
          DOMAIN_IF_INVALID_ARGUMENT);
    fixture.availability.items[0].functionId = DOMAIN_FUNCTION_CLIMATE;
    CHECK(DomainIf_TxMobileFunctionAvailability(&fixture.availability) ==
          DOMAIN_IF_INVALID_ARGUMENT);
    fixture.digitalKeyState.setting = (DigitalKeySetting_t)99;
    CHECK(DomainIf_TxMobileDigitalKeyState(&fixture.digitalKeyState) ==
          DOMAIN_IF_INVALID_ARGUMENT);
    fixture.digitalKeyResult.actualUnlockTransition = true;
    CHECK(DomainIf_TxMobileDigitalKeyResult(&fixture.digitalKeyResult) ==
          DOMAIN_IF_INVALID_ARGUMENT);
    fixture.settings.hasNormalLightLevel = true;
    fixture.settings.normalLightLevelPercent = 101U;
    CHECK(DomainIf_TxMobileUserSettings(&fixture.settings) ==
          DOMAIN_IF_INVALID_ARGUMENT);
}

static void Test_WindowPayloadValidationStillApplies(void)
{
    TxFixtures_t fixture;

    InitTxFixtures(&fixture);
    fixture.windowCommand.targetPositionPercent = 101U;
    CHECK(DomainIf_TxWindowCommand(&fixture.windowCommand) ==
          DOMAIN_IF_INVALID_ARGUMENT);
    fixture.windowState.positionPercent = 101U;
    CHECK(DomainIf_TxMobileWindowState(&fixture.windowState) ==
          DOMAIN_IF_INVALID_ARGUMENT);
    fixture.mobileResult.target = MOBILE_RESULT_TARGET_WINDOW;
    CHECK(DomainIf_TxMobileRequestResult(&fixture.mobileResult) ==
          DOMAIN_IF_UNSUPPORTED);
}

static void Test_LocalOkDoesNotCreateExecutionEvidence(void)
{
    TxFixtures_t fixture;
    DomainIf_TxPort_t port = MakeCompleteTxPort();

    InitTxFixtures(&fixture);
    ResetTxObservations();
    g_txReturns[TX_INDEX_BCM_DOOR] = DOMAIN_IF_OK;
    CHECK(DomainIf_SetTxPort(&port) == DOMAIN_IF_OK);
    CHECK(DomainIf_TxBcmDoorCommand(&fixture.doorCommand) == DOMAIN_IF_OK);
    CHECK(g_txCalls[TX_INDEX_BCM_DOOR] == 1U);
    /* Core owns no Command/Result state and only exposes the callback result. */
    CHECK(DomainIfCore_TestHasTxPort());
}

int main(void)
{
    RUN_TEST(Test_InitialState);
    RUN_TEST(Test_PreInitRegistrationRejected);
    RUN_TEST(Test_PreInitTxRejected);
    RUN_TEST(Test_FirstInitialization);
    RUN_TEST(Test_ObserverNullRegistrationAndUnbind);
    RUN_TEST(Test_TxPortNullRegistrationAndUnbind);
    RUN_TEST(Test_ObserverIsCopiedByValue);
    RUN_TEST(Test_TxPortIsCopiedByValue);
    RUN_TEST(Test_SerializedReregistrationReplacesTables);
    RUN_TEST(Test_DuplicateInitPreservesBindings);
    RUN_TEST(Test_ObserverAndTxPortAreIndependent);
    RUN_TEST(Test_AllTxNullInputsRejected);
    RUN_TEST(Test_TxPortAndCallbackAvailability);
    RUN_TEST(Test_AllTxWrappersForwardAndPreserveStatus);
    RUN_TEST(Test_TxPortCopyAndReregistration);
    RUN_TEST(Test_CommandValidationAndRequestContext);
    RUN_TEST(Test_StateAndResultValidation);
    RUN_TEST(Test_WindowPayloadValidationStillApplies);
    RUN_TEST(Test_LocalOkDoesNotCreateExecutionEvidence);

    (void)printf("Executed %u tests, %u failed checks.\n",
                 g_executedTests, g_failedChecks);
    return (g_failedChecks == 0U) ? 0 : 1;
}
