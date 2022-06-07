#!/usr/bin/env python3

import sys
import json
import struct
from collections import defaultdict


msg_type = {
    0x0000: 'HELLO',
    0x0001: 'SHUTDOWN',
    0x1000: 'LOCK_REQUEST',
    0x1001: 'LOCK_RELEASE',
    0x1002: 'VOTE_REQUEST',
    0x1003: 'TX_END',
    0x1004: 'TX_ABORT',
    0x2000: 'LOCK_GRANT',
    0x2001: 'VOTE_RESPONSE',
    0x2002: 'TRANSACTION_END_RESPONSE'
}


class P4DBPacket:
    def __init__(self, ts, data):
        self.timestamp = float(ts)
        self.__data = data

    def __getattr__(self, key):
        val = self.__data.get(f'p4db.{key}')
        if key == 'msgtype':
            return msg_type.get(int(val), 'UNKOWN_TYPE')
        return val


class Analyzer:
    def __init__(self):
        self.pairs = {}
        self.avg = []
        self.min = 0xffffffff
        self.max = 0
        self.types = defaultdict(int)

    def process_pkt(self, packet):
        # if packet.msgtype == 'LOCK_REQUEST':
        #     key = (packet.transactionNumber, packet.lockId)
        #     self.pairs[key] = packet.timestamp
        # elif packet.msgtype == 'LOCK_GRANT':
        #     key = (packet.transactionNumber, packet.lockId)
        #     rtt = packet.timestamp - self.pairs[key]
        #     del self.pairs[key]

        #     rtt *= 1e6
        #     # print(f'rtt={rtt:8.2f} µs')
        #     self.min = min(self.min, rtt)
        #     self.max = max(self.max, rtt)
        #     self.avg.append(rtt)

        #     if len(self.avg) == 1024:
        #         avg_rtt = sum(self.avg) / len(self.avg)
        #         print(f'LockReq<->LockGrant RTT    min={self.min:8.2f}µs  max={self.max:8.2f}µs  avg={avg_rtt:8.2f}µs')
        #         self.avg = []
        #         self.min = 0xffffffff
        #         self.max = 0

        self.types[packet.msgtype] += 1

    def __del__(self):
        for k, v in self.types.items():
            print(f'{k} --> {v} times')

# tshark -X lua_script:/p4db/dissector.lua -r ~/dump.pcap -T json | python pcap_timing.p


if __name__ == '__main__':
    analyzer = Analyzer()

    START = '  {'
    END = '  },'
    obj = []
    for line in sys.stdin:
        line = line.rstrip()
        if line == START:
            obj = []
        obj.append(line)
        if line == END:
            packet = json.loads(''.join(obj)[:-1])
            p4db_pkt = P4DBPacket(packet['_source']['layers']['frame']
                                  ['frame.time_epoch'], packet['_source']['layers']['p4db'])
            analyzer.process_pkt(p4db_pkt)
