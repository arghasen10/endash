import traceback as tb
import os



def getTraceBack(exc_info):
    error = "pid:" + str(os.getpid()) + " ppid:" + str(os.getppid()) + "\n"
    error += str(exc_info[0]) + "\n"
    error += str(exc_info[1]) + "\n\n"
    error += "\n".join(tb.format_tb(exc_info[2]))
    return error

