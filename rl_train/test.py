import simenv.PowerEnv as PE
import simenv.PowerMeasure as PME
import sys
import pdb
from abr.BOLA import BOLA
from abr.FastMPC import AbrFastMPC
from abr.Pensiev import AbrPensieve
from abr.RobustMPC import AbrRobustMPC
import json
import os
import argparse
import shutil


def parseArg():
    parser = argparse.ArgumentParser(description='Run Experiment')
    parser.add_argument('--mode',  help='Select mode of running', default="measure", choices=["measure", "train", "test"])
    parser.add_argument('--abr', help='Select abr', default='BOLA', choices=['BOLA', 'Pensiev', 'FastMPC', 'RobustMPC'])
    parser.add_argument('--interval', help='Select interval', type=int, default=30)
    args = parser.parse_args()

    os.environ["LOW_POW_ENV_BUFFER_INTERVAL"] = str(args.interval)

    return args


def runExperiment(env, abr):
    try:
        env.experimentSimpleOne(abr)
    except:
        trace = sys.exc_info()
        pdb.set_trace()


if __name__ == "__main__":
    args = parseArg()
#     print(os.environ["LOW_POW_ENV_BUFFER_INTERVAL"])
    abr = BOLA
    if args.abr == "BOLA":
       abr=BOLA
    elif args.abr == "FastMPC":
       abr=AbrFastMPC
    elif args.abr == "Pensiev":
       abr=AbrPensieve
    elif args.abr == "RobustMPC":
       abr=AbrRobustMPC
    else:
        assert False and "Invalid ABR"

    env = PE
    if args.mode == "measure":
        env = PME
    if args.mode == "train":
        env = PE
        if os.path.isdir("ResModel/rnnBuffer/"):
            shutil.rmtree("ResModel/rnnBuffer/")
        os.environ["LOW_POW_ENV_BUFFER_READ_ONLY"] = "0"
    if args.mode == "test":
        env = PE
        os.environ["LOW_POW_ENV_BUFFER_READ_ONLY"] = "1"

    runExperiment(env, abr)



#os.environ["LOW_POW_ENV_BUFFER_INTERVAL"] runExperiment(PE, BOLA)
# runExperiment(PME, BOLA)
