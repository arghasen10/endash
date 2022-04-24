import numpy as np
import os
import sys

if len(sys.argv) < 4:
    print("Use {} <outputFileis> <columns> <input] ..".format(sys.argv[0]))
    exit(1)

columns = [int(x) for x in sys.argv[2].split(",")]
opfiles = [x for x in sys.argv[1].split(",")]
assert len(opfiles) == len(columns)
res = [[] for x in columns]

for fid, fpath in enumerate(sys.argv[3:]):
    with open(fpath) as fp:
        dt = [[float(x) for x in y.strip().split()] for y in fp if y[0] != "#"]
        dt = np.array(dt)
        for i, col in enumerate(columns):
            v = np.var(dt[:, col])
            s = np.std(dt[:, col])
            m = np.mean(dt[:, col])
            mx = np.max(dt[:, col])
            mn = np.min(dt[:, col])
            res[i].append((fid, v, s, m, mx, mn))

for i, col in enumerate(columns):
    with open(opfiles[i], "w") as fp:
        print("#sz v, s, m max min", file=fp)
        for x in res[i]:
            print(*x, file=fp)


# python3 measureMeanVar.py results/GroupSize/early_inc.dat,results/GroupSize/normal_inc.dat 1,2 results/GroupSizePlot/earlydownload/GroupP2PTimeoutInc_{3..10}.dat
# python3 measureMeanVar.py results/GroupSize/early_rnn.dat,results/GroupSize/normal_rnn.dat 1,2 results/GroupSizePlot/earlydownload/GroupP2PEnvTimeoutIncRNN_{3..10}.dat
# python3 measureMeanVar.py results/GroupSize/upload_rnn.dat,results/GroupSize/download_rnn.dat 1,2 results/GroupSizePlot/upload_download/GroupP2PEnvTimeoutIncRNN_{3..10}.dat
# python3 measureMeanVar.py results/GroupSize/upload_inc.dat,results/GroupSize/download_inc.dat 1,2 results/GroupSizePlot/upload_download/GroupP2PTimeoutInc_{3..10}.dat
# python3 measureMeanVar.py results/GroupSize/qoe_inc.dat 1 results/GroupSizePlot/QoE/GroupP2PTimeoutInc_{3..10}.dat
# python3 measureMeanVar.py results/GroupSize/qoe_rnn.dat 1 results/GroupSizePlot/QoE/GroupP2PEnvTimeoutIncRNN_{3..10}.dat
# python3 measureMeanVar.py results/fairness/intra,results/fairness/inter 0,1 results/GenPlots/fairness/GrpDeter.dat
