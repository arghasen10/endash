


def getObj(arg=None, **kw):
    if arg is not None and len(kw) != 0:
        raise Exception("Invalid arguments")
    if arg is None:
        if len(kw) > 0:
            arg = kw
    if type(arg) == dict:
        return Obj(arg)
    elif type(arg) == list:
        return [getObj(x) for x in arg]
    else:
        return arg

def getJson(arg):
    if type(arg) == Obj:
        return arg.getDict()
    elif type(arg) == list:
        return [getJson(x) for x in arg]
    else:
        return arg

class Obj:
    def __init__(self, arg):
        if type(arg) != dict:
            raise Exception("need dict")
        self.dt = {}
        for x, y in arg.items():
            self.dt[x] = getObj(y)

    def __getattr__(self, name):
        if name == "dt" or name not in self.dt:
            raise AttributeError(name)
        return self.dt[name]

    def __setattr__(self, name, value):
        if name == "dt":
            super().__setattr__(name, value)
            return
        self.dt[name] = value

    def __getitem__(self, key):
        return self.dt[key]

    def keys(self):
        return self.dt.keys()

    def setattrs(self, **kw):
        for k,v in kw.items():
            self.dt[k] = v

    def getattr(self, key, *v):
        if len(v) == 0:
            return self.__getattr__(key)
        return self.dt.get(key, v[0])

    def getDict(self):
        return {k: getJson(v) for k, v in self.dt.items()}

    def print(self):
        print(self.dt.items())

    def __str__(self):
        return str(self.dt)
