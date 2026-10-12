# CAN 공통 Stack — 두 메시지 TX/RX 상세 Flow

PROPOSED_PRIVATE / DESIGN_ONLY. U4C 구현/Host 실행/보드 실행 NOT_EXECUTED; G4D_INDEPENDENT=PENDING. 기준 9e0b1e5d97d32971bb0863e144dd2ef515f2ccf4.

[16함수 색인](../05_CAN_COMMON_FUNCTION_INDEX.md) · [Flow 색인](../04_FLOW_INDEX.md) · [기존 U2 CIS_REAR Flow](10_CIS_REAR_FLOW.md) · [기존 U2 Permission Flow](20_DOMAIN_PERMISSION_FLOW.md)

<a id="tx-path"></a>
## TX — current intent 수용과 전송 수용 분리

1. Node App/U2 기존 ingress가 owner contract를 확인한다. 공개 `NetCisRear`/Permission에는 wireBoot/TX가 없으므로 임의 rawcast하지 않는다. Unit/producer 미합의는 기존 affected binding만 NOT_READY; 명시 I1 raw Host diagnostic 입력은 private 후보 Submit로 설계할 수 있다.
2. Service Submit validates raw ranges/source origin/time, caller 데이터를 fixed pending[2]에 deep copy하여 stable commit한다. caller buffer는 반환 후 수정가능. `NET_SUBMIT_OK`는 이 지점뿐이며 P accepted를 의미하지 않는다. current samekey는 아직 Port미수용pending만 교체한다.
3. owner cycle은 Transport Pump→Service Pump. Service는 RR key당최대1/총2TX시도, 원UPDATE/payload/quality보존, 이전anchor→now의AGE만1회추가하고 메시지별TXcandidate를선택한다. 미합의producer/clockorigin은publicprojection 보류; raw합법품질진단은완성.
4. Transport 즉시 Codec scratch encode→fixed T reservation/copy→Port TryTx. P 독립owned20B/fulltoken 수용시 CAN_ACCEPTED이며 T INFLIGHT와Service 해당key TXnext만commit한다. NO_START BUSY/NOT_READY/NO_CAPACITY는 T reservationrollback, caller accepted-out 및Service TXnext불변. accepted bytes immutable.
5. Port PollEvent→Transport slot당 retained terminal→Service TakeOutcome. COMPLETE는 local controller 종결, CONFIRMED_FAIL은 확인된미전송실패, UNKNOWN은 전송가능성/결과불명. UNKNOWN Pbuffer는실quiescence까지핀. peerRX/AppResult는 이경로에없으며 U2Node기능callback 정책을임의추가하지않는다.

<a id="rx-path"></a>
## RX — bytes/원형/출처/typed 전달

1. ISR/backend callback은 bytes20/flags/actualDLC/Clock/fullbackendgeneration을 Powned ring에 bounded copy 후 publish한다. borrowed pointer를 callback return뒤읽지않는다. PortTerminal은 별도retainedcell이므로 RXfull이 terminal을지우지않는다.
2. Transport Pump가 Port PollEvent를 owner에서 bounded drain하고 currentgeneration RX만Townedring에copy한다. RT full dropnew count, 기능Fault없음. TakeRx는 callerstack record로copy/pop; reset후oldRXquarantine. oldTXterminal은livefulltoken 기준이므로 oldgen일괄폐기금지.
3. Codec Decode가 fixed ID2/IDE0 FDF1 BRS1 RTR0/DLC11/20B/used19/pad0/I1 raw 범위검사 후 namedI1 fields를 atomic출력한다. Frame raw AGE/TX/UPDATE/Boot는 그대로다. CRC신설/sizeofcast/unknownEnumnormalization없음.
4. Service OnDecoded는 explicit owner-admitted Boot/context, source별TX/UPDATE 반범위순서, 동일UPDATEpayloadcoherence, 원AGE watermark를검사한다. busactivity/validframe/newTX/newUPDATE는별도진단, peer상태기한/functionalpermission150ms확정없음. RXlocalclock는 출생시간이아니고 다른MCUclock를빼지않는다.
5. accepted rawdiagnostic 관측과 optional diagnostic callback은 Nodeowner문맥이며 callback반환은App기능성공증거가아니다. rear0.1cm→C2mm·Permission4ECU기능binding은미합의OPEN유지, WINDOW DEFERRED 실제liveness미대상. 기존 U2실제7함수에기능확정/새공개인자를추가하지않는다.

<a id="success-boundaries"></a>
## 다섯 성공 경계

| 경계 | 근거 | 증명되는 범위 |
|---|---|---|
| NET_SUBMIT_OK | Service stable owned copy | 로컬 제출 수용 |
| CAN_ACCEPTED | Port owned frame/token commit | 전송요청 수명 인수 |
| terminal COMPLETE | backend release-safe 종결 기록 | local controller 완료 |
| peer RX | 해당 peer 실수신 별도자료 | 해당 peer의 해당frame 수신 |
| App/Vehicle Result | 기능 owner 원 result 계약 | 실행/최종판정, U4D 생성금지 |

<a id="capacity-time-recovery"></a>
## bounded 소유권·시간·복구

Svc pending2/currentonly→T NT bounded slot+outcome1/slot→P NT 독립frame+terminal1/slot; RX P RP→T RT→ownerstack. malloc0; no shared mutable singleton. future discrete/query은지원하지않고 silentoverwrite 금지/FIFOidentity/orderedresponse 조건만유지한다. 전체RAM은 각instance/배열+최대scratch를합하고 checkedsize와providerbudget을검사한다. Host NT2/RP4/RT4/pending2/budget8는 TEST_PROFILE_ONLY이며 target숫자확정없음.

Clock sameepoch+valid+finitegap<2^31 근거 있을 때만u32delta, AGE는u16포화. rawAGE가65535이면fresh증거없고간격불명은UNCERTAIN/noTX. sameUPDATE는 retainedAGE가낮아지지않게 현재동일시점까지계상후max. preacceptwait1회, encode→actualTX/peerRX는frozenbytes에추가불가하며unmeasuredlowerbound/uncertain라진단. 5ms는강제cancel/정확hop증거아니고50/150ms확정없음.

Watchdog/clockbreak는P Reset1회→미확정acceptedUNKNOWN+generationfence, 미소비확정terminal보존. T는Pterminal을fulltoken으로인수, late callback새slot적용금지. generation/serial64MAX는wrap없이NOT_READY. quiescence/oldTXdrain증거전Pbytespin/admissionoff. 복구후preacceptedcurrent는원UPDATE/AGE계속, acceptedoldframe 자동재전송없음. ISR은capture/publish만, owner는TransportPump→SvcPump. reentrant/criticalterminalloss가fakeDONE/FAIL/CLEAR/ALLOW를만들지않는다.

<a id="design-test-matrix"></a>
## CAN-D01~22 — 예약된 U4C 기대값

모두 **DESIGN_ONLY / NOT_EXECUTED**. 아래수치는독립입력예제이지실제테스트결과가아니다. U4C에서 sentinel-out 바이트 비교, token/event/state 관측, sourceage 독립계산으로 실행검증한다.

| Test-ID | 독립입력/trigger | 기대 상태·반환·관측점 | 참여경계 | 결정/OPEN |
|---|---|---|---|---|
| CAN-D01 | rear Boot1 TXraw99 update7 age10; submit now100/accounted100; mutate caller distance | Svc owned update7/distance original, pending age10; caller 수정 영향0; NET_SUBMIT_OK는stablecopy만 | Submit→TryTx→PortTryTx | U4D-DEC-01 |
| CAN-D02 | permission raw unknown255 quality2 age655; public producer 미합의 | private raw diagnostics quality/AGE그대로; publicPermission NOT_READY; ALLOW 생성0 | Submit/Encode/OnDecoded | OPEN-PERM |
| CAN-D03 | NULL, actual length19/21, DLC10/12, IDE/RTR=1 FDF/BRS=0/2 | CAN_INVALID_ARG/PROFILE_REJECT; Codec output sentinel bytes 불변, pending/slot partialcommit0 | Encode/Decode/TryTx | U4D-DEC-02 |
| CAN-D04 | B19=1, Boot0, invalid enum; 정상validity0/reasonraw1 별도 | padding/code는reject; 구조상합법degraded는CAN_OK, 기능fresh/ALLOW 없음 | Encode/Decode | U4D-DEC-02 |
| CAN-D05 | pendingcapacity1 다른key; NT2 occupied; Pbusy/notready | NO_CAPACITY/BUSY/NOT_READY; source/header/out 불변, reserve rollback; serial만monotone소비가능 | Submit/TryTx/PortTryTx | U4D-DEC-01 |
| CAN-D06 | 같은key update7→8 accepted전/후 submit | 전: owned pending 교체; 후: P/T immutableframe 유지하며 새pending만수용 | Submit/Pump/TryTx | U4D-DEC-01 |
| CAN-D07 | 미지원discrete/query kind, identity있다고가정 | CAN_UNSUPPORTED beforecopy; 구현추가0; futureFIFO nooverwrite/orderedresponse 계약만 | Submit | OPEN-WIRE |
| CAN-D08 | Submit OK; P ACCEPTED; local COMPLETE; peerRX 없음 | 5성공경계개별. current outcome는local만; 4ECU수신/vehicleDONE 합성0 | Pump/TakeOutcome/PollEvent | U4D-DEC-03 |
| CAN-D09 | accepted token(1,0,7), confirmed fail/unknown; 늦은complete | 한localterminal1회, 후속성공승격0; UNKNOWN PbufferPINNED | Pump/TakeOutcome/Reset | U4D-DEC-04 |
| CAN-D10 | busoff before/afteraccepted; recovery currentpending | admissionoff; unresolvedacceptedUNKNOWN; quiescence전NOT_READY; preacceptedpending age 증가보존, oldaccepted자동retry0 | Pump/Reset/PollEvent | OPEN-PORT-CAPABILITY |
| CAN-D11 | generation1 slot0serial7 reset→2; callbackoldtoken;serialMAX | liveoldtoken은해당oldTslot에만인수; reusedslot mismatch진단; MAX wrap중지 | TryTx/Pump/Reset | U4D-DEC-05 |
| CAN-D12 | unknownBoot2 vs admittedBoot1; duplicate/oldTX; otherkind | OnDecoded quarantine/orderreject, kind별context분리, 다른ECUalive합성0 | BindSource/OnDecoded | OPEN-PEER-OWNER |
| CAN-D13 | TX/UPDATE fffffffe→1 diff3; diff80000000; clock fffffff0→10 | boundedrate/windowproof일때만newer/delta32; half/epoch/horizon미증명NO_TX/quarantine | Submit/OnDecoded/Pump | OPEN-TIME |
| CAN-D14 | age65534 now+2; raw65535 pluswait; invalidclock | sat65535유지·SATmetadata·freshclaim0; invalidclockUNCERTAIN/NO_TX | Submit/Pump/OnDecoded | U4D-DEC-06 |
| CAN-D15 | sameupdate7 retransmit age0 after100ms; 새TX oldUPDATE | max(originage+elapsed,incomingage+elapsed), UPDATE유지; no origin-age reset/newobservation | Submit/OnDecoded | U4D-DEC-06 |
| CAN-D16 | RP4/RT4 ISR burst5번째; callbackbuffer 즉시overwrite | first4 ownedcopy,5drop-new counter; terminal보존독립; functionFAIL/CLEAR0 | PollEvent/Pump/TakeRx | U4D-DEC-07 |
| CAN-D17 | 같은orderedinputs superloop/worker budget8 동일cycles | TransportPump→SvcPump동일FSM; notificationcount와 messagecount는 별개; SDKsymbolCore0 | Pump/PollEvent | U4D-DEC-07 |
| CAN-D18 | malformedpadding와validdegradedquality 번갈아RX | reject경로·rawvalidquality경로독립; reject는semanticliveness갱신0 | Decode/OnDecoded | U4D-DEC-02 |
| CAN-D19 | callbacksync/reentrant/duplicate/lostterminal | RESERVED latch후accepted; guardREENTRANT; lostcriticaladmissionoff/UNKNOWNfence; 결과두번0 | PortTryTx/PollEvent/Reset/Pump | U4D-DEC-04 |
| CAN-D20 | rear raw0.1cm vs C2mm Unit미합의, sentinel65535 | raw그대로; 자동곱/cast·CLEAR·fault환산0; affectedprojectionHOLD | Decode/OnDecoded | OPEN-UNIT |
| CAN-D21 | permissioncoherence/quality/sourceLimit미합의 | raw진단가능; functionalproducerbindingNOT_READY; 50/150ms or ALLOW 확정0 | Submit/OnDecoded | OPEN-PERM |
| CAN-D22 | A/B two config+storage+knownBoot 별도 instance | pending/token/rxhealth/counter 상호독립; globalsingleton없음 | Init/BindSource/Pump | U4D-DEC-08 |

## 기존 기준과 구현 경계

기존 U2두Flow의 모든7함수behavior/pseudo를보존한다. 링크된CISSubmit/DOMAINPermissionproducer/DOMAINrearreceive/CIS+BCM+VSSprivatepermission 소비 및 U2LinkPort는 이미 승인된안전차단/원본규칙. 위private candidate와의 연결은Call/Data flow proposal로만기술한다. U3rawdecode/encode/format은기존diagnostic API이고runtimeDriverSend/Receive로바꾸지않는다. 실제재사용은기존I1 4CANcodec함수와Golden data이며C/H복제·개작0. 공개 API/producers SDK ABI확정없이후속U4C Host Mock으로검증가능한scope를명시한다.
