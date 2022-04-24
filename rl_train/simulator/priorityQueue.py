
def parent(i):
    if i == 0:
        raise ValueError("root don't have parent")
    j = i + 1
    p = int(j/2)
    return p -1

def right(i):
    j = i + 1
    r = j*2 + 1
    return r - 1

def left(i):
    j = i + 1
    r = j * 2
    return r -1

class PriorityQueue:
    def __init__(self):
        self.heap = []
        self.index = {}
        self.count = 0
        self.minium = 0

    def parent(self, i):
        return (i-1)/2

    def swap(self, i, j):
        self.heap[i], self.heap[j] = self.heap[j], self.heap[i]
        self.index[self.heap[i][1]] = i
        self.index[self.heap[j][1]] = j

    def insert(self, k, val = None):
        if val is None:
            val = k
        self.heap.append((k, self.count, val))
        self.index[self.count] = len(self.heap)-1
        self.count += 1
        i = len(self.heap) - 1
        while i > 0 and self.heap[parent(i)][0] > self.heap[i][0]:
             self.swap(parent(i), i)
             i = parent(i)
        return self.count-1


    # Method to remove minium element from min heap
    def extractMin(self):
        if len(self.heap) <= 0:
            raise ValueError("Heap is empty")
        if len(self.heap) == 1:
            x = self.heap.pop()
            del self.index[x[1]]
            return x[2]

        self.swap(len(self.heap) - 1, 0)
        root = self.heap.pop()
        self.minHeapify(0)
        del self.index[root[1]]
        return root[2]

    # This functon deletes key at index i. It first reduces
    # value to minus infinite and then calls extractMin()
    def delete(self, ref):
        index = self.index[ref]
        self.swap(index, len(self.heap)-1)
        x = self.heap.pop()
        del self.index[x[1]]
        self.minHeapify(index)
        return x[2]

    def isRefExists(self, ref):
        return ref in self.index

    def minHeapify(self, i = 0):
        while True:
            l = left(i)
            r = right(i)
            smallest = i
            hs = len(self.heap)
            if l < hs and self.heap[l][0] < self.heap[i][0]:
                smallest = l
            if r < hs and self.heap[r][0] < self.heap[smallest][0]:
                smallest = r

            if smallest == i:
                break

            self.swap(smallest, i)
            i = smallest

    def isEmpty(self):
        return len(self.heap) == 0

    # Get the minimum element from the heap
    def peekMin(self):
        return self.heap[0][2]


if __name__ == "__main__":
    h = PriorityQueue()
    h.insert(3);
    print(h.heap, h.index)
    h.insert(2);
    print(h.heap, h.index)
    h.delete(1);
    print(h.heap, h.index)
    h.insert(15);
    print(h.heap, h.index)
    h.insert(5);
    print(h.heap, h.index)
    h.insert(4);
    print(h.heap, h.index)
    h.insert(45);
    print(h.heap, h.index)
    while not h.isEmpty():
        print(h.extractMin())
        print(h.heap, h.index)
