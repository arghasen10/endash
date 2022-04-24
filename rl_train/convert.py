import sys
import time

# fp = open(sys.argv[1])
# line = fp.readline().strip().split(",")
#
# for i, x in enumerate(line):
#     print(i, x)

def parseData(line):
    line = line.strip().split(",")
    tm = time.strftime("%s", time.strptime(line[7], "%H:%M:%S"))
    return float(tm), float(line[42]) * 8 / 1000000



def main():
    dt = []

    with open(sys.argv[1]) as fp:

        dt = [parseData(x) for i, x in enumerate(fp) if i != 0]

        stTime = dt[0][0]-1
        print(stTime)
        with open(sys.argv[2], "w") as fp:
            print("0", "0", file=fp)
            for x in dt:
                print(x[0]-stTime, x[1], file=fp)

def printHead():
    with open(sys.argv[1]) as fp:

        dt = fp.readline().strip().split(",")
        for i, n in enumerate(dt):
            print(f"{i:3}->{n:20}")


def parseArg(experiments):
    global EXIT_ON_CRASH, MULTI_PROC
    parser.add_argument('-i', '--input',  help='Input file name', nargs=1, required=True)
    parser.add_argument('-o', '--output',  help='Output file name', nargs=1, required=False)
    parser.add_argument('-h', '--headers-only',  help='Print the first row', action="store_true", required=False)
    parser.add_argument('-c', '--cols',  help='columns to be printed', default=[7, 41])
    parser.add_argument('exp', help=experiments, nargs='+')
    args = parser.parse_args()
    EXIT_ON_CRASH = args.exit_on_crash
    MULTI_PROC = not args.no_slave_proc
    if "EXP_ENV_LEARN_PROC_QUALITY" in os.environ:
        del os.environ["EXP_ENV_LEARN_PROC_QUALITY"]
    if "EXP_ENV_LEARN_PROC_AGENT" in os.environ:
        del os.environ["EXP_ENV_LEARN_PROC_AGENT"]
    if args.no_quality_rnn_proc:
        os.environ["EXP_ENV_LEARN_PROC_QUALITY"] = "NO"
    elif args.no_agent_rnn_proc:
        os.environ["EXP_ENV_LEARN_PROC_AGENT"] = "NO"

    return args.exp




if __name__ == "__main__":
    print(sys.argv)
    if '-h' in sys.argv:
        printHead()

    else:
        main()
