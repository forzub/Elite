import math, sys
from sim import *
def common(aHerr, aZerr, vh, g, r):
    aH=aHerr/4.0; aZ=aZerr/3.0+g-vh*vh/r
    cap=0.95*MAIN
    if math.hypot(aH,aZ)>cap:
        z=max(-cap,min(cap,aZ)); room=math.sqrt(max(0,cap*cap-z*z)); aH=max(-room,min(room,aH))
        aZ=z
    return aH,aZ
def q3(st,x,h,vh,vz,g,r):
    if not st:
        st['a0']=max(x,ENDARC+1000); st['h0']=max(h,HH+100); st['b']=max(0.5,(vh*vh-VEND**2)/(2*(st['a0']-ENDARC)))
    X=max(0,min(1,(x-ENDARC)/(st['a0']-ENDARC))); dh=st['h0']-HH
    gh=HH+dh*X*X*(3-2*X); slope=dh*6*X*(1-X)/(st['a0']-ENDARC)
    vhw=min(math.sqrt(VEND**2+2*st['b']*max(0,x-ENDARC)), math.sqrt(MU/r))
    vzw=max(-400,min(60,-max(vh,0)*slope+(gh-h)/8))
    if h<0.6*HH: vzw=max(vzw,0)
    return common(vhw-vh, vzw-vz, vh, g, r)
def straight(st,x,h,vh,vz,g,r, aV=2.0, rate=3.0):
    # aim straight at the hand-over point, uniform braking along the arc, vertical limited by
    # what can be stopped (aV) and ramped (rate m/s2)
    if not st:
        st['a0']=max(x,ENDARC+1000); st['b']=max(0.5,(vh*vh-VEND**2)/(2*(st['a0']-ENDARC))); st['vzw']=vz
    vhw=min(math.sqrt(VEND**2+2*st['b']*max(0,x-ENDARC)), math.sqrt(MU/r))
    tgo=max(10,(x-ENDARC)/max(vh,30))
    vzw=-(h-HH)/tgo
    vzw=max(vzw,-math.sqrt(2*aV*max(0,h-0.8*HH)))
    vzw=max(-400,min(60,vzw))
    p=st['vzw']; d=0.05*rate; vzw=max(p-d,min(p+d,vzw)); st['vzw']=vzw
    return common(vhw-vh, vzw-vz, vh, g, r)
V={'q3':q3,'straight':straight}
if __name__=="__main__": print(run(V[sys.argv[1]], log=len(sys.argv)>3))
