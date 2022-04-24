
SPEED_TOLARANCE_PERCENT = 100

class Group():
    GROUP_ID = 0
    def __init__(s, ql, network, lonePeer = False):
        s.id = Group.GROUP_ID
        Group.GROUP_ID += 1
        s.nodes = []
        s._schedules = {}
        s.segMents = 0
        s.nodeAddedWithSegId = {}
        s.qualityLevel = ql
        s.network = network
        s.lonePeer = lonePeer

    def __schedule(s, segId):
        nodeslist = list(s.nodes)
        nodeslist.sort(key=lambda x: - s.nodeAddedWithSegId[x])
        for i, seg in enumerate(range(segId, 1000)):
            x = i % len(nodeslist)
            s._schedules[seg] = nodeslist[x]

    def notifyNodes(s, segId):
        for n in s.nodes:
            n.schedulesChanged(segId, s.nodes, s._schedules)

    @property
    def schedule(s):
        return s._schedules

    def numNodes(s):
        return len(s.nodes)

    def add(s, node, segId = 0):
        if node in s.nodes:
            return
        s.nodes.append(node)
        s.nodeAddedWithSegId[node] = segId
        s.__schedule(segId)

    def remove(s, node, segId = 0):
        if node not in s.nodes:
            return
        s.nodes.remove(node)
        del s.nodeAddedWithSegId[node]
        if len(s.nodes):
            s.__schedule(segId)

    def currentSchedule(s, node, segId):
        if node not in s.nodes:
            raise Exception("Node not in the group")
        if segId not in s._schedules:
            return
        if segId < s.nodeAddedWithSegId[node]:
            return
        downloader = s._schedules[segId]
        return downloader

    def getAllNode(s, *excepts): # set substraction probably good but it will be random
        return [x for x in s.nodes if x not in excepts]

    def isNeighbour(s, node):
        return node in s.nodeAddedWithSegId

    def isSuitable(self, node):
        if self.lonePeer:
            return False
        if not self.network: return True
        for n in self.nodes:
            if not self.network.isClose(node.networkId, n.networkId):
                return False
        return True

    def QoEVariation(s):
        qoes = []
        for n in s.nodes:
            qoe = n._vAgent.QoE
            qoes.append(qoe)
        qoesvar = [abs(qoes[x-1]-q) for x,q in enumerate(qoes) if x != 0]
        avg = sum(qoesvar)/len(qoesvar)
        return avg


    def jainsFairnessQoEIndex(s):
        qls = []
        for n in s.nodes:
            ql = n._vAgent.bitratePlayed
            ql.reverse()
            qls.append(ql)
        qls = list(zip(*qls))
        qls = list(zip(*qls))
        assert len(qls) == len(s.nodes)
        avgs = [sum(x)/len(x) for x in qls]

        jfi = sum(avgs)**2 / (sum([x**2 for x in avgs]))/len(avgs)
        return jfi

    def jainsFairnessDownloadTime(s):
        dlTimes = [n._vTotalWorkingTime for n in s.nodes]

        jfi = sum(dlTimes)**2 / (sum([x**2 for x in dlTimes]))/len(dlTimes)
        return jfi

class GroupManager():
    def __init__(self, peersPerGroup = 3, defaultQL = 3, videoInfo = None, network = None):
        self.groups = {}
        self.groupBuckets = {}
        self.peers = {}
        self.peersPerGroup = peersPerGroup
        self.defaultQL = defaultQL
        self.videoInfo = videoInfo
        self.network = network

    def getId(s, node):
        if node not in s.peers:
            raise Exception("node not found")
        return s.peers[node].id


    def getQoEVariation(self, saturated = True):
        QoEVar = []
        for x, grps in self.groups.items():
            for grp in grps:
                if not saturated or grp.numNodes() == self.peersPerGroup:
                    QoEVar.append(grp.QoEVariation())
        return QoEVar

    def getGroupFairness(self, saturated = True):
        fairnessIndeces = []
        for x, grps in self.groups.items():
            for grp in grps:
                if not saturated or grp.numNodes() == self.peersPerGroup:
                    fairnessIndeces.append(grp.jainsFairnessQoEIndex())
        return sum(fairnessIndeces)/len(fairnessIndeces)

    def getGroupFairnessDownloadTime(self, saturated = True):
        fairnessIndeces = []
        for x, grps in self.groups.items():
            for grp in grps:
                if not saturated or grp.numNodes() == self.peersPerGroup:
                    fairnessIndeces.append(grp.jainsFairnessDownloadTime())
        return sum(fairnessIndeces)/len(fairnessIndeces)

    def getInterGroupFairness(self, saturated = True):
        fairnessIndeces = []
        for x, grps in self.groups.items():
            nodes = []
            for grp in grps:
                if not saturated or grp.numNodes() == self.peersPerGroup:
                    nodes += grp.getAllNode()
            if len(nodes) == 0:
                continue
            qls = []
            for n in nodes:
                ql = n._vAgent.bitratePlayed
                ql.reverse()
                qls.append(ql)
            qls = list(zip(*qls))
            qls = list(zip(*qls))
            assert len(qls) == len(nodes)
            avgs = [sum(x)/len(x) for x in qls]

            jfi = sum(avgs)**2 / (sum([x**2 for x in avgs])) / len(avgs)
            fairnessIndeces.append(jfi)
        return sum(fairnessIndeces)/len(fairnessIndeces)

    def getInterGroupFairnessDownloadTime(self, saturated = True):
        fairnessIndeces = []
        for x, grps in self.groups.items():
            nodes = []
            for grp in grps:
                if not saturated or grp.numNodes() == self.peersPerGroup:
                    nodes += grp.getAllNode()
            if len(nodes) == 0:
                continue

            dlTimes = [n._vTotalWorkingTime for n in nodes]

            jfi = sum(dlTimes)**2 / (sum([x**2 for x in dlTimes])) / len(dlTimes)
            fairnessIndeces.append(jfi)
        return sum(fairnessIndeces)/len(fairnessIndeces)


    def add(s, node, segId = 0, ql = -1):
        ql = s.defaultQL if ql < 0 else ql

        conn = node.connectionSpeedBPS

        lonePeer = False

        if s.videoInfo:
            connQl = ql
            connTol = conn * (1 + SPEED_TOLARANCE_PERCENT/100.0)
            while connQl > 0:
                if connTol >= s.videoInfo.bitrates[connQl]:
                    break
                connQl -= 1
#             if ql != connQl:
#                 print("assiging ql:", connQl, "instead of", ql)
            ql = connQl

            if ql == 0 and connTol < s.videoInfo.bitrates[ql]:
                lonePeer = True  #there will be no other group for this peer

        group = None
        for grp in s.groups.get(ql, []):
            assert grp.qualityLevel == ql
            if grp.numNodes() < s.peersPerGroup and grp.isSuitable(node):
                group = grp
                break

        if not group:
            group = Group(ql, s.network, lonePeer)
            grps = s.groups.setdefault(ql, [])
            grps.append(group)

        s.peers[node] = group
        group.add(node, segId)
        s.adjustBuckets(group, segId)

    def adjustBuckets(s, grp, segId):
        ql = grp.qualityLevel
        bucket = s.groupBuckets.setdefault(ql, {})
        lastCount = 0
        for c in bucket:
            if grp in bucket[c]:
                lastCount = c
                bucket[c].remove(grp)
                break

        cnt = grp.numNodes()

        grpSet = bucket.setdefault(cnt, set())
        grpSet.add(grp)

        grp.notifyNodes(segId)


    def remove(s, node, segId = 0):
        if node not in s.peers:
            return
        grp = s.peers[node]
        grp.remove(node, segId)
        if grp.numNodes() == 0:
            grps = s.groups.setdefault(grp.qualityLevel, [])
            grps.remove(grp)
        del s.peers[node]
        s.adjustBuckets(grp, segId)

    def currentSchedule(s, node, segId):
        if node not in s.peers:
            return
        return s.peers[node].currentSchedule(node, segId)

    def isNeighbour(s, me, node):
        if me not in s.peers:
            return False
        return s.peers[me].isNeighbour(node)

    def getQualityLevel(s, node):
        if node not in s.peers:
            raise Exception("node not found")
        return s.peers[node].qualityLevel

    def isLonepeer(s, node):
        if node not in s.peers:
            raise Exception("node not found")
        return s.peers[node].lonePeer

    def getRtt(self, node1, node2):
        return self.network.getRtt(node1.networkId, node2.networkId)

    def transmissionTime(self, node1, node2, size):
        if not self.network:
            raise Exception("No p2p")
        return self.network.transmissionTime(node1.networkId, node2.networkId, size)

    def getAllNode(self, node, *excepts): # set substraction probably good but it will be random
        return self.peers[node].getAllNode(*excepts)

    def getSchedule(self, node): # set substraction probably good but it will be random
        return self.peers[node].schedule

    def printGroupBucket(self):
        print(self.groupBuckets)
