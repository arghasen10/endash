import json
import numpy as np

def defaultEncoder(o):
    if isinstance(o, np.int64): return int(o)
    raise TypeError


def dump(*arg, default=None, **kwarg):
    if not default:
        default=defaultEncoder
    return json.dump(*arg, default=default, **kwarg)

def dumps(*arg, default=None, **kwarg):
    if not default:
        default=defaultEncoder
    return json.dumps(*arg, default=default, **kwarg)

def loads(*arg, **kwarg):
    return json.loads(*arg, **kwarg)

def load(*arg, **kwarg):
    return json.load(*arg, **kwarg)

