#!/usr/bin/python
# Takes as input:
#    List of edges: end1 end2 cost
# Produces as output:
#    a file for each router, endi.cfg, and a model file
#    routers containing routername hostname baseport
import string

def addedge(routers, left, right, cost):
    if left not in routers.keys():
        routers[left] = {}
    if right not in routers.keys():
        routers[right] = {}
    rightLink = len(routers[right]) + 1
    leftLink = len(routers[left]) + 1
    print left, leftLink, right, rightLink
    routers[left][right] = (cost, rightLink)
    routers[right][left] = (cost, leftLink)

def genFiles(test):
    f = open(test+'/links')
    lines = f.readlines()
    f.close()
    routers = {}
    for line in lines:
        if line[0]=='#': continue
        words = string.split(line)
        if len(words)<3: continue
        addedge(routers, words[1], words[0], int(words[2]))
    print routers
    for router in routers.keys():
        f = open(test+ '/' + router + '.cfg', "w")
        r = routers[router]
        for dest in r.keys():
            line = '%s %d %d %d\n' % (dest, r[dest][0], routers[dest][router][1],
                                      r[dest][1])
            f.write(line)
        f.close()
    f = open(test+'/routers', "w")
    port = 20000
    for router in routers.keys():
        line = '%s localhost %d\n' % (router, port)
        f.write(line)
        port = port + 10
    f.close()

if __name__ == '__main__':
    import sys
    genFiles(sys.argv[1])
