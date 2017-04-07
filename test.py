#!/usr/bin/env python2.7

import subprocess
import sys

def table(res):
    # print "| {0:^20} | {1:^20} | {2:^20} | {3:^20} |".format("NITEMS", "SORT", "TIME", "SPACE")
    # for r in res:
    #     print "| {0:^20} | {1:^20} | {2:^20} | {3:^20} |".format(r[0], r[1], r[2][0], r[2][1])
    for r in res:
        print "Result for command [ {} {} {} {} ]  is [ Result: {} Page_Faults:{} Disk_Reads:{} Disk_Writes:{} ]\n".format(r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7])

def main():
    results = []
    pairs = [(4, 4), (4, 3), (3, 4), (20, 15), (25, 15), (15, 25), (50, 50)]#, (40, 60), (60,40), (75, 80), (80, 75), (100, 90), (90, 100), (100, 100)]
    for npage, nframe in pairs:
        for method in ["rand", "fifo", "custom"]:
            for program in ["scan", "sort", "focus"]:
                input = "./virtmem {} {} {} {}".format(npage, nframe, method, program)
                print "testing", input
                result = subprocess.check_output(input.split())
                output = []
                for x in result.split('\n'):
                    i = 3 if len(x.split()) > 3 else 2
                    if x:
                        output.append(x.split()[i])
                output = [npage, nframe, method, program] + output
                results.append(output)

    table(results)
if __name__ == '__main__':
    main()