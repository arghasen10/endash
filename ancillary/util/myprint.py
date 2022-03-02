import os
import sys

P2P_PRINT_ENV_ARG = "P2P_PRINT_MODE"

P2P_PRINT_MODE_ALL = "ALL"
P2P_PRINT_MODE_ERROR = "ERR"
P2P_PRINT_MODE_NON_STD = "NOSTD"
P2P_PRINT_MODE_NONE = "NONE"

def myprint(*args, **kwargs):
    mode = os.environ.get(P2P_PRINT_ENV_ARG, P2P_PRINT_MODE_ALL)
    if mode not in [P2P_PRINT_MODE_ALL, P2P_PRINT_MODE_ERROR, P2P_PRINT_MODE_NON_STD, P2P_PRINT_MODE_NONE]:
        mode = P2P_PRINT_MODE_ALL

    if mode == P2P_PRINT_MODE_NONE:
        return
    if mode == P2P_PRINT_MODE_ERROR:
        if "file" in kwargs and kwargs["file"] == sys.stderr:
            print(*args, **kwargs)
        return
    if mode == P2P_PRINT_MODE_NON_STD:
        if "file" in kwargs and kwargs["file"] not in [sys.stderr, sys.stdout]:
            print(*args, **kwargs)
        return
    print(*args, **kwargs)


if __name__ == "__main__":
    myprint("hello")
    os.environ[P2P_PRINT_ENV_ARG] = ""
    myprint("hello 1")
    os.environ[P2P_PRINT_ENV_ARG] = P2P_PRINT_MODE_ERROR
    myprint("hello 2")
    myprint("hello 2 err", file=sys.stderr)
    
    os.environ[P2P_PRINT_ENV_ARG] = P2P_PRINT_MODE_NONE
    myprint("hello 3")
    myprint("hello 3 err", file=sys.stderr)
