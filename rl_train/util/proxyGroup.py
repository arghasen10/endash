import util.group
from util import p2pnetwork
import networkx as nx
import numpy as np

def getComb(l, elems):
    if len(elems) == 0:
        return []

    res = [(elems[0],)*l]
    if len(elems) == 1:
        return res

    res = []
    curEle = elems[0]
    nexEle = elems[1:]
    res.append((curEle,)*l)

    for x in range(l):
        child = getComb(l - x, nexEle)
        for c in child:
            p = (curEle,)*x + c
            assert len(p) == l
            res.append(p)
    return res

DEFAULT_QL = 1

class ProxyGroupManager(util.group.GroupManager):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

    def add(s, node, segId = 0, ql = -1):
        group = None
        ql = DEFAULT_QL
        for grp in s.groups.get(ql, []):
            assert grp.qualityLevel == ql
            if grp.numNodes() < s.peersPerGroup and grp.isSuitable(node):
                group = grp
                break

        if not group:
            group = util.group.Group(ql, s.network, False)
            grps = s.groups.setdefault(ql, [])
            grps.append(group)

        s.peers[node] = group
        group.add(node, segId)
        s.adjustBuckets(group, segId)



class ProxyP2PNetwork(p2pnetwork.P2PNetwork):
    def __init__(self, grpSize):
        self.grp = nx.Graph()
        self.setupNodes(grpSize)

    def setupNodes(self, grpSize):
        distances = [1,2,3,4]

        assert grpSize > 1

        numEdge = grpSize*(grpSize-1)/2
        combs = getComb(int(numEdge), distances)

        edges = [(x,y) for x in range(grpSize) for y in range(x, grpSize) if y != x]
        assert len(edges) == numEdge

        nodes = []

        for i, cmb in enumerate(combs):
            for x, ed in enumerate(edges):
                a,b = ed
                self.grp.add_edge(a + i*grpSize, b+i*grpSize, rtt = cmb[x])

    def getDistance(self, n1, n2):
        dt = self.grp.get_edge_data(n1, n2)
        if not dt:
            return 1000
        return dt['rtt']

    def isClose(self, n1, n2):
#         return True
        dist = self.getDistance(n1, n2)
        return dist < 100 #threshold


class ProxyNode():
    def __init__(self, nid):
        self.networkId = nid
    def schedulesChanged(self, *arg, **k):
        pass

def main():
    grpSize = 3
    p = ProxyP2PNetwork(grpSize)
    nodes = list(p.nodes())
    grp = ProxyGroupManager(peersPerGroup = grpSize, network = p)
    np.random.shuffle(nodes)

    for x in nodes:
        pnode = ProxyNode(x)
        grp.add(pnode)

    print([len(g) for key, g in grp.groups.items()])
    print(grp.groupBuckets)
    for g in grp.groups.get(DEFAULT_QL, []):
        print([x.networkId for x in g.getAllNode()])


if __name__ == "__main__":
    main()
