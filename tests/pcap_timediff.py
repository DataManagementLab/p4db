#!/usr/bin/env python3

import sys
import dpkt
import pathlib


def pairs(iterable):
    a = iter(iterable)
    return zip(a, a)



def avg_rtt(filename):
    f = open(filename, 'rb')
    pcap = dpkt.pcap.Reader(f)

    timestamps = []
    for (req_ts, req_buf), (res_ts, res_buf) in pairs(pcap):
        req_eth = dpkt.ethernet.Ethernet(req_buf)
        res_eth = dpkt.ethernet.Ethernet(res_buf)

        assert(req_eth.type == 0x1000 and res_eth.type == 0x1000)
        assert(req_eth.src == res_eth.dst)
        assert(req_eth.dst == res_eth.src)

        ts_diff = (res_ts - req_ts) * 1000000

        assert(req_buf[-8:] == res_buf[-8:])

        timestamps.append(ts_diff)

    avg_us = sum(timestamps) / len(timestamps)
    return min(timestamps), avg_us, max(timestamps)



def avg_rtt2(filename):
    f = open(filename, 'rb')
    pcap = dpkt.pcap.Reader(f)

    timestamps = [0] * 101
    for ts, buf in pcap:
        eth = dpkt.ethernet.Ethernet(buf)

        txn_id = int.from_bytes(buf[-8:], byteorder='little') - 1
        ts *= 1000000

        if eth.src == b'\xac\x1fkAe\x0b':
            if txn_id <= 100:
                timestamps[txn_id] = ts
        else:
            # assert(ts >= timestamps[txn_id])
            timestamps[txn_id] = ts - timestamps[txn_id]

            if txn_id == 100:
                break
        # assert(req_eth.type == 0x1000 and res_eth.type == 0x1000)
        # assert(req_eth.src == res_eth.dst)
        # assert(req_eth.dst == res_eth.src)

        # ts_diff = (res_ts - req_ts) * 1000000

        # assert(req_buf[-8:] == res_buf[-8:])

        # timestamps.append(ts_diff)

    print(timestamps)

    timestamps = list(ts for ts in timestamps if ts != -1)
    print(len(timestamps))
    avg_us = sum(timestamps) / len(timestamps)
    return min(timestamps), avg_us, max(timestamps)


initial_us = None
for filename in sys.argv[1:]:
    min_us, avg_us, max_us = avg_rtt(filename)

    if initial_us is None:
        initial_us = avg_us
        suffix = ''
    else:
        increase = (avg_us - initial_us) / initial_us
        suffix = f'{increase:+.3%}'

    print(f'{pathlib.Path(filename).name:16} Min RTT: {min_us:.4f}µs Avg RTT: {avg_us:.4f}µs Max RTT: {max_us:.4f}µs {suffix:>16}')