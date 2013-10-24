#!/usr/bin/python
import readrouters
import socket

def change(test, end1, end2, newcost):
    routerInfo = readrouters.readrouters(test)

    end1info = (routerInfo[end1].host, routerInfo[end1].baseport)
    end2info = (routerInfo[end2].host, routerInfo[end2].baseport)

    msg1 = 'L %s %d' % (end2, newcost)
    msg2 = 'L %s %d' % (end1, newcost)
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.sendto(msg1, end1info)
    s.sendto(msg2, end2info)

if __name__=='__main__':
    import sys
    change(sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4])) 
