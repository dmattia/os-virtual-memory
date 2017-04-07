#!/usr/bin/env python2.7

import subprocess
import sys
import numpy as np
import matplotlib.pyplot as plt

def single_plot(x, y, title):
  fig = plt.figure()

  ax = fig.add_subplot(111)
  ax.set_xlabel("Number of frames")
  ax.set_ylabel(title.split('_')[-1])
  fig.suptitle(title, fontsize=14, fontweight='bold')

  plt.scatter(x, y)

  plt.savefig('images/' + title + '.png')
  plt.close()

def plot(res):
  methods = ["rand", "fifo", "custom"]
  programs = ["scan", "sort", "focus"]

  for method in methods:
    for program in programs:
      print("Method: " + method + " and program: " + program)
      results = [r for r in res if r[2] == method and r[3] == program]

      frame_counts = [int(r[1]) for r in results]

      page_faults = [int(r[-3]) for r in results]
      title = method + '_' + program + '_PageFaults'
      single_plot(frame_counts, page_faults, title)

      disk_reads = [int(r[-2]) for r in results]
      title = method + '_' + program + '_DiskReads'
      single_plot(frame_counts, disk_reads, title) 

      disk_writes = [int(r[-1]) for r in results]
      title = method + '_' + program + '_DiskWrites'
      single_plot(frame_counts, disk_writes, title)
  
def main():
    results = []
    frames = np.linspace(3, 100, 10)
    pairs = [(100, int(frame)) for frame in frames]
    for npage, nframe in pairs:
        for method in ["rand", "fifo", "custom"]:
            for program in ["scan", "sort", "focus"]:
                input = "./virtmem " + str(npage) + " " + str(nframe) + " " + method + " " + program
                print "testing", input
                result = subprocess.Popen(input.split(), stdout=subprocess.PIPE).communicate()[0]
                output = []
                for x in result.split('\n'):
                    i = 3 if len(x.split()) > 3 else 2
                    if x:
                        output.append(x.split()[i])
                output = [npage, nframe, method, program] + output
                results.append(output)

    plot(results)

if __name__ == '__main__':
    main()
