#!/usr/bin/python
import readrouters
import socket

def printTables(routerInfo):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    for router in routerInfo.keys():
        endinfo = (routerInfo[router].host, routerInfo[router].baseport)
        msg = 'P'
        s.sendto(msg, endinfo)

def printTable(router, routerInfo):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    endinfo = (routerInfo[router].host, routerInfo[router].baseport)
    msg = 'P'
    s.sendto(msg, endinfo)


if __name__=='__main__':
    import sys
    routerInfo = readrouters.readrouters(sys.argv[1])
    if len(sys.argv) == 3:
        printTable(sys.argv[2], routerInfo)
    elif len(sys.argv) == 2:
        printTables(routerInfo)
