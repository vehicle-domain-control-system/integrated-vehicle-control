"""Add the Network include paths and defines to the Debug_FLASH C compiler of a CIS S32DS project.

NETWORK_INTEGRATION.md 2: the 7 include paths and 11 defines of the Network CIS bench
(nodes/cis/bench/s32k144_cis/README.md), plus src/net_bench, src/cis and CIS_USE_NETWORK_API.
Only the first configuration (Debug_FLASH) is changed; entries already present are kept once.
Close the project in S32DS (or refresh it afterwards) so the IDE does not write back an old copy.

Default is a dry run.
  python firmware/CIS/tools/add_network_to_cproject.py <project>
  python firmware/CIS/tools/add_network_to_cproject.py <project> --apply
"""
from __future__ import annotations
import argparse, pathlib, sys

INCLUDES = [
    'src/network/include',
    'src/network/src/l04_frame_codec',
    'src/network/src/l02_typed_handoff/m03_typed_handoff',
    'src/network/src/l05_transport_port/m07_buffer_outcome',
    'src/network/src/l05_transport_port/m09_observation',
    'src/network/src/l03_message_service/m04_message_service',
    'src/network/src/platform/rtd',
    'src/net_bench',
    'src/cis',
]
DEFINES = [
    'NETWORK_UPPER_NODE_PROFILE=1',
    'NET_NODE_BUILD',
    'NET_BOARD_ISR_TICK_HOOK=Board_TickMs32',
    'NET_BOARD_TICK_TO_MS_HOOK=Board_TickToMs32',
    'NET_BOARD_NODE_CONFIG_HOOK=Board_NetNodeConfig',
    'NET_BOARD_US_HOOK=Board_Us32',
    'NET_BOARD_CAN_CALLBACK=DebugCan_EventCallback',
    'NET_BOARD_CAN_ERROR_CALLBACK=DebugCan_ErrorCallback',
    'NET_BOARD_UART_C_INSTANCE=1U',
    'NET_BOARD_UART_C_CONFIG=Lpuart_Uart_Ip_xHwConfigPB_1',
    'NET_BOARD_UART_CALLBACK=DebugConsole_UartCallback',
    'CIS_USE_NETWORK_API',
]
# Last entries of the generated Debug_FLASH lists; the new entries go right after them.
INCLUDE_ANCHOR = '<listOptionValue builtIn="false" value="&quot;${ProjDirPath}/board&quot;"/>'
DEFINE_ANCHOR = '<listOptionValue builtIn="false" value="CPU_CORTEX_M4F"/>'
INDENT = '\n\t\t\t\t\t\t\t\t\t'


def entry(value):
    return '<listOptionValue builtIn="false" value="%s"/>' % value


def insert_after(text, anchor, values, end_tag):
    """Insert the missing values after the first anchor, inside its option (up to end_tag)."""
    i = text.find(anchor)
    if i < 0:
        raise ValueError('anchor not found: ' + anchor)
    i += len(anchor)
    block = text[i:text.index(end_tag, i)]
    missing = [v for v in values if entry(v) not in block]
    return text[:i] + ''.join(INDENT + entry(v) for v in missing) + text[i:], missing


def main():
    p = argparse.ArgumentParser(description=__doc__.split('\n', 1)[0])
    p.add_argument('project', type=pathlib.Path, help='CIS S32DS project folder')
    p.add_argument('--apply', action='store_true', help='write .cproject (default: dry run)')
    args = p.parse_args()

    f = args.project / '.cproject'
    if not f.is_file():
        print('ERROR: %s not found' % f)
        return 2
    text = f.read_text(encoding='utf-8')
    if 'CPU_S32K144' not in text:
        print('ERROR: %s has no CPU_S32K144 define (not an S32K144 project?)' % f)
        return 2
    try:
        text, inc = insert_after(text, INCLUDE_ANCHOR, ['${ProjDirPath}/' + x for x in INCLUDES], '</option>')
        text, defs = insert_after(text, DEFINE_ANCHOR, DEFINES, '</option>')
    except ValueError as e:
        print('ERROR: %s (generated .cproject layout changed?)' % e)
        return 2
    for v in inc:
        print('  include %s' % v)
    for v in defs:
        print('  define  %s' % v)
    print('  new includes %d, new defines %d' % (len(inc), len(defs)))
    if args.apply and (inc or defs):
        f.write_text(text, encoding='utf-8')
    print('done.' if args.apply else 'dry run: nothing written (use --apply).')
    return 0


if __name__ == '__main__':
    sys.exit(main())
