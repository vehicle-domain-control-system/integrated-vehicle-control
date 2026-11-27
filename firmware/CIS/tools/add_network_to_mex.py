"""Add the Network links to the .mex of a new S32K144 RTD project (NETWORK_INTEGRATION.md 1-B).

A new S32DS project (S32K144 LQFP100, RTD 3.0.0) has only Clock, Port and OsIf. This script adds:
  Pins         CAN0 PTE4/PTE5, LPUART1 PTC6/PTC7
  FlexCAN      FLEXCAN_0, CAN FD ISO + BRS, 64 B, 7 MB, 500 kbit/s / 2 Mbit/s at 48 MHz,
               callbacks DebugCan_EventCallback / DebugCan_ErrorCallback
  Lpuart_Uart  LPUART_1, 115200, callback DebugConsole_UartCallback (Lpuart_Uart_Ip_xHwConfigPB_1)
  IntCtrl_Ip   CAN0 (MB 0..15, ORed, error) and LPUART1 interrupts
The names are those of the Network debug node, so the defines of NETWORK_INTEGRATION.md 2 apply.
firmware/CIS/s32ds/cis_node.mex is the result for the project made on 2026-10-29.

Then generate the code (S32DS: Update Code, or headless with a workspace that S32DS does not hold open):
  s32dsc.exe -noSplash -data <scratch_ws> -application com.nxp.swtools.framework.application
      -Load <project>/<name>.mex -HeadlessTool Pins -ExportSrc <project>
      -HeadlessTool Peripherals -ExportSrc <project>

Default is a dry run.
  python firmware/CIS/tools/add_network_to_mex.py <project>/<name>.mex
  python firmware/CIS/tools/add_network_to_mex.py <project>/<name>.mex --apply
"""
import argparse, pathlib, sys

p = argparse.ArgumentParser(description=__doc__.split('\n', 1)[0])
p.add_argument('mex', type=pathlib.Path, help='.mex of the S32DS project')
p.add_argument('--apply', action='store_true', help='write the file (default: dry run)')
args = p.parse_args()

mex = args.mex
t = mex.read_text(encoding='utf-8')
if '<instance name="FlexCAN"' in t or 'peripheral="CAN0"' in t:
    print('%s already has FlexCAN / CAN0: nothing changed.' % mex)
    sys.exit(0)

PINS = '''                  <pin peripheral="CAN0" signal="can0_rx" pin_num="9" pin_signal="PTE4">
                     <pin_features>
                        <pin_feature name="direction" value="INPUT"/>
                     </pin_features>
                  </pin>
                  <pin peripheral="CAN0" signal="can0_tx" pin_num="8" pin_signal="PTE5">
                     <pin_features>
                        <pin_feature name="direction" value="OUTPUT"/>
                     </pin_features>
                  </pin>
                  <pin peripheral="LPUART1" signal="lpuart1_rx" pin_num="81" pin_signal="PTC6">
                     <pin_features>
                        <pin_feature name="direction" value="INPUT"/>
                     </pin_features>
                  </pin>
                  <pin peripheral="LPUART1" signal="lpuart1_tx" pin_num="80" pin_signal="PTC7">
                     <pin_features>
                        <pin_feature name="direction" value="OUTPUT"/>
                     </pin_features>
                  </pin>
'''

FLEXCAN = '''                  <instance name="FlexCAN" uuid="0b31199a-ae00-44b5-9221-cf7680bb25ae" type="FlexCAN" type_id="FlexCAN" mode="ip" enabled="true" comment="" custom_name_enabled="false" editing_lock="false">
                     <config_set name="FlexCAN">
                        <setting name="Name" value="FlexCAN"/>
                        <struct name="ConfigTimeSupport">
                           <setting name="POST_BUILD_VARIANT_USED" value="false"/>
                           <setting name="IMPLEMENTATION_CONFIG_VARIANT" value="VARIANT-PRE-COMPILE"/>
                        </struct>
                        <array name="FlexCAN_Configurations">
                           <struct name="0">
                              <setting name="CanHwChannel" value="FLEXCAN_0"/>
                              <setting name="Name" value="FlexCAN_Config0"/>
                              <setting name="max_num_mb" value="7"/>
                              <setting name="num_id_filters" value="FLEXCAN_RX_FIFO_ID_FILTERS_8"/>
                              <setting name="is_rx_fifo_needed" value="false"/>
                              <setting name="fd_enable" value="true"/>
                              <setting name="flexcanMode" value="FLEXCAN_NORMAL_MODE"/>
                              <setting name="payload" value="FLEXCAN_PAYLOAD_SIZE_64"/>
                              <setting name="transfer_type" value="FLEXCAN_RXFIFO_USING_INTERRUPTS"/>
                              <setting name="rxFifoDMAChannel" value="0"/>
                              <setting name="extCbtEnable" value="false"/>
                              <setting name="bitRateSwitch" value="true"/>
                              <setting name="CanControllerFdISO" value="true"/>
                              <setting name="CanControllerAutoBusOff" value="true"/>
                              <setting name="CanRemoteRequestStore" value="false"/>
                              <setting name="CanClockFromBus" value="true"/>
                              <setting name="CanLowestBufferTransmittedFirst" value="false"/>
                              <setting name="timeStampSurce" value="FLEXCAN_CAN_CLK_TIMESTAMP_SRC"/>
                              <setting name="pe_clock_frequency" value="48000000"/>
                              <struct name="flexcan_time_segment">
                                 <setting name="Name" value="FlexCAN_Ip_TimeSeg0"/>
                                 <setting name="flexcan_cfg_propSeg" value="7"/>
                                 <setting name="flexcan_cfg_phaseSeg1" value="5"/>
                                 <setting name="flexcan_cfg_phaseSeg2" value="3"/>
                                 <setting name="flexcan_cfg_preDivider" value="6"/>
                                 <setting name="flexcan_cfg_rJumpwidth" value="3"/>
                              </struct>
                              <struct name="flexcan_time_segment_cbt">
                                 <setting name="Name" value="FlexCAN_Ip_TimeSegCBT0"/>
                                 <setting name="flexcan_cfg_propSeg" value="14"/>
                                 <setting name="flexcan_cfg_phaseSeg1" value="4"/>
                                 <setting name="flexcan_cfg_phaseSeg2" value="5"/>
                                 <setting name="flexcan_cfg_preDivider" value="1"/>
                                 <setting name="flexcan_cfg_rJumpwidth" value="4"/>
                              </struct>
                              <setting name="FlexCanCallback" value="DebugCan_EventCallback"/>
                              <setting name="FlexCanErrorCallback" value="DebugCan_ErrorCallback"/>
                              <setting name="num_enhanced_std_id_filters" value="0"/>
                              <setting name="num_enhanced_ext_id_filters" value="0"/>
                              <setting name="num_enhanced_watermark" value="0"/>
                              <setting name="is_enhanced_rx_fifo_needed" value="false"/>
                           </struct>
                        </array>
                        <struct name="FlexCAN_General">
                           <setting name="Name" value="FlexCAN_General"/>
                           <setting name="FlexCANEnableUserModeSupport" value="false"/>
                           <setting name="FlexCANEnableTimeStampSupport" value="false"/>
                           <setting name="FlexCANIpDevErrorDetect" value="true"/>
                        </struct>
                        <struct name="timeout">
                           <setting name="Name" value="FlexCAN_Timeout"/>
                           <setting name="timeout_type" value="OSIF_COUNTER_DUMMY"/>
                           <setting name="timeout_value" value="10000"/>
                        </struct>
                        <array name="FlexCAN_Pn_Configurations"/>
                     </config_set>
                  </instance>
'''

LPUART = '''                  <instance name="Lpuart_Uart" uuid="3d198d0e-a162-4dd2-9fe9-a2a462b104d3" type="Lpuart_Uart" type_id="Lpuart_Uart" mode="ip" enabled="true" comment="" custom_name_enabled="false" editing_lock="false">
                     <config_set name="Lpuart_Uart">
                        <setting name="Name" value="Lpuart_Uart"/>
                        <struct name="ConfigTimeSupport" quick_selection="Default">
                           <setting name="POST_BUILD_VARIANT_USED" value="false"/>
                           <setting name="IMPLEMENTATION_CONFIG_VARIANT" value="VARIANT-PRE-COMPILE"/>
                        </struct>
                        <struct name="GeneralConfiguration">
                           <setting name="Name" value="GeneralConfiguration"/>
                           <setting name="UartDevErrorDetect" value="true"/>
                           <setting name="UartTimeoutMethod" value="OSIF_COUNTER_DUMMY"/>
                           <setting name="UartTimeoutDuration" value="10000000"/>
                           <setting name="UartDmaEnable" value="false"/>
                           <setting name="UartCallbackCapability" value="true"/>
                           <array name="UartCallback">
                              <setting name="0" value="DebugConsole_UartCallback"/>
                           </array>
                           <array name="UartCallbackParam"/>
                        </struct>
                        <struct name="UartGlobalConfig">
                           <setting name="Name" value="UartGlobalConfig"/>
                           <array name="UartChannel">
                              <struct name="0">
                                 <setting name="Name" value="UartChannel_0"/>
                                 <setting name="UartHwUsing" value="LPUART_IP"/>
                                 <setting name="UartClockFunctionalGroupRef" value="ClockConfig0"/>
                                 <struct name="DetailModuleConfiguration">
                                    <setting name="Name" value="DetailModuleConfiguration"/>
                                    <setting name="UartHwChannel" value="LPUART_1"/>
                                    <setting name="DesireBaudrate" value="LPUART_UART_BAUDRATE_115200"/>
                                    <setting name="CustomBaudrateMantissa" value="1"/>
                                    <setting name="CustomBaudrateDivisor" value="4"/>
                                    <setting name="UartInteruptDmaMethod" value="LPUART_UART_IP_USING_INTERRUPTS"/>
                                    <array name="UartDmaTxChannelRef"/>
                                    <array name="UartDmaRxChannelRef"/>
                                    <setting name="UartParityType" value="LPUART_UART_IP_PARITY_DISABLED"/>
                                    <setting name="UartStopBitNumber" value="LPUART_UART_IP_ONE_STOP_BIT"/>
                                    <setting name="UartWordLength" value="LPUART_UART_IP_8_BITS_PER_CHAR"/>
                                    <setting name="UartInternalLoopbackEnable" value="false"/>
                                    <setting name="UartTimeoutEnable" value="false"/>
                                 </struct>
                              </struct>
                           </array>
                        </struct>
                     </config_set>
                  </instance>
'''

def isr(i, irq, handler):
    return '''                                 <struct name="%d">
                                    <setting name="Name" value="PlatformIsrConfig_%d"/>
                                    <setting name="IsrName" value="%s"/>
                                    <setting name="IsrEnabled" value="true"/>
                                    <setting name="IsrPriority" value="0"/>
                                    <setting name="IsrHandler" value="%s"/>
                                 </struct>
''' % (i, i, irq, handler)

INTCTRL = '''                  <instance name="IntCtrl_Ip" uuid="f74229e4-c6cc-4d7a-900f-1555e0a4ce09" type="IntCtrl_Ip" type_id="IntCtrl_Ip" mode="ip" enabled="true" comment="" custom_name_enabled="false" editing_lock="false">
                     <config_set name="IntCtrl_Ip">
                        <setting name="Name" value="IntCtrl_Ip"/>
                        <struct name="ConfigTimeSupport">
                           <setting name="POST_BUILD_VARIANT_USED" value="false"/>
                           <setting name="IMPLEMENTATION_CONFIG_VARIANT" value="VARIANT-PRE-COMPILE"/>
                        </struct>
                        <struct name="IntCtrlConfigGeneral">
                           <setting name="Name" value="IntCtrlConfigGeneral"/>
                           <setting name="IntCtrlDevErrorDetect" value="true"/>
                           <setting name="PlatformEnableUserModeSupport" value="false"/>
                        </struct>
                        <array name="IntCtrlConfig">
                           <struct name="0">
                              <setting name="Name" value="IntCtrlConfig_0"/>
                              <array name="PlatformIsrConfig">
''' + isr(0, 'CAN0_ORed_0_15_MB_IRQn', 'CAN0_ORED_0_15_MB_IRQHandler') \
    + isr(1, 'CAN0_ORed_IRQn', 'CAN0_ORED_IRQHandler') \
    + isr(2, 'CAN0_Error_IRQn', 'CAN0_Error_IRQHandler') \
    + isr(3, 'LPUART1_RxTx_IRQn', 'LPUART_UART_IP_1_IRQHandler') + '''                              </array>
                           </struct>
                        </array>
                     </config_set>
                  </instance>
'''

def once(old, new):
    global t
    assert t.count(old) == 1, old
    t = t.replace(old, new)

# pins: after the PTA0 pin entry
once('''                  </pin>
               </pins>''', '''                  </pin>
''' + PINS + '''               </pins>''')
# peripherals: before the end of the instance list
once('''               </instances>
            </functional_group>''', FLEXCAN + LPUART + INTCTRL + '''               </instances>
            </functional_group>''')
if args.apply:
    mex.write_text(t, encoding='utf-8')
    print('patched %s' % mex)
else:
    print('dry run: %s would get 4 pins and FlexCAN, Lpuart_Uart, IntCtrl_Ip (use --apply).' % mex)
