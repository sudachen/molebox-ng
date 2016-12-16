
#
#
#  Copyright (c) 2008-2009, Alexey Sudachen
#
#

import re, random, os, os.path, rfc822, sys

TEGGOCORE = os.environ['TEGGOCORE']
if not TEGGOCORE: TEGGOCORE = 'c:/Projects/teggocore'
sys.path.append(TEGGOCORE+'/Core/pylib/Py')

from system import Ffind

def Main():

    force = False
    last_number  = 1;
    strings = {}
    if os.path.exists('data/.messages'):
        fm = open('data/.messages',"r")
        last_number = int(fm.readline().strip())
        for l in fm.xreadlines():
            i = l.find(':')
            lN = int(l[0:i].strip())
            lS = l[i+1:].strip()
            strings[lN] = lS.decode('utf8')
        fm.close()
    else:
        force = True

    for i in Ffind("*.cpp"):
        do_rename = False
        f_name = i[0]
        print f_name
        f = open(f_name,"r")
        fo = open(f_name+'.tr~',"w+")
        for l in f.xreadlines():
            j = l
            while True:
                m = re.search(r'Tr\((\d*),"((?:(\\")|[^"])*)"\)',j)
                if m:
                    do_rename = True
                    lN = int(m.group(1))
                    lS = m.group(2)
                    #print force, lN,lS
                    if lN == 0 or force:
                        lN = last_number
                        last_number += 1
                        strings[lN] = unicode(lS)
                    fo.write(j[0:m.start()])
                    fo.write('Tr(%d,"%s")'%(lN,lS))
                    j = j[m.end():]
                    continue
                break
            fo.write(j)
        fo.close()
        f.close()
        if do_rename:
            os.unlink(f_name)
            os.rename(f_name+'.tr~',f_name)
        else:
            os.unlink(f_name+'.tr~')

    fm = open('data/.messages',"w+")
    fm.write(str(last_number))
    fm.write('\n')
    for l,j in strings.items():
        fm.write("%d:%s\n"%(l,j.encode('utf8')))
    fm.close()

if __name__ == '__main__':
    Main()
