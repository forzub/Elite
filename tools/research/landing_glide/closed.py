import math, sys
MU=4.9048695e12; R=1737400.0+4108.0
P1=math.radians(15); P2=math.radians(-20)
VEND=450.0; ARC_END=15000.0; H_END=8000.0
def ph2(x,h,vh,vz,T2,dt=2.0):
    t=0
    while t<1500:
        r=R+h; defi=MU/r/r-vh*vh/r
        if math.hypot(vh,vz)<=VEND or h<0: break
        vh+=-T2*math.cos(P2)*dt; vz+=(-T2*math.sin(P2)-defi)*dt; x-=vh*dt; h+=vz*dt; t+=dt
    return x,h
def t2_for(x,h,vh,vz):
    # the phase-2 thrust that stops at ARC_END (more thrust = shorter)
    lo,hi=1.0,60.0
    for i in range(18):
        m=0.5*(lo+hi); xe,he=ph2(x,h,vh,vz,m)
        if xe<ARC_END: lo=m
        else: hi=m
    return 0.5*(lo+hi)
def ph1_end(x,h,vh,vz,T1,dt=2.0):
    # fly phase 1 with T1 until phase 2 at the nominal thrust would stop at ARC_END; the end height
    t=0
    while t<1500:
        xe,he=ph2(x,h,vh,vz,10.0,dt=4.0)
        if xe<=ARC_END: break
        r=R+h; defi=MU/r/r-vh*vh/r
        vh+=-T1*math.cos(P1)*dt; vz+=(-T1*math.sin(P1)-defi)*dt; x-=vh*dt; h+=vz*dt; t+=dt
        if h<0: return -1e9
    xe,he=ph2(x,h,vh,vz,10.0)
    return he
def t1_for(x,h,vh,vz):
    lo,hi=0.0,15.0   # more T1 (nose up) = lower end
    for i in range(14):
        m=0.5*(lo+hi); he=ph1_end(x,h,vh,vz,m)
        if he>H_END: lo=m
        else: hi=m
    return 0.5*(lo+hi)
def sim(arc0, log=False, dt=0.1, rate=math.radians(15)):
    x,h,vh,vz=arc0,82707.0,1638.98,0.0; th=0.0; phase=1; t=0; T=0; replan=0; mn=mx=0
    while t<1500:
        if t>=replan:
            replan=t+1.0
            if phase==1:
                xe,he=ph2(x,h,vh,vz,10.0,dt=4.0)
                if xe<=ARC_END: phase=2
            if phase==1: T=t1_for(x,h,vh,vz); P=P1
            else: T=t2_for(x,h,vh,vz); P=P2
        d=max(-rate*dt,min(rate*dt,P-th)); th+=d
        gate=1.0 if abs(th-P)<math.radians(5) else 0.0
        r=R+h; defi=MU/r/r-vh*vh/r
        vh+=-T*gate*math.cos(th)*dt; vz+=(-T*gate*math.sin(th)-defi)*dt; x-=vh*dt; h+=vz*dt; t+=dt
        mn=min(mn,math.degrees(th)); mx=max(mx,math.degrees(th))
        if log and int(t/dt)%int(20/dt)==0: print(f"t {t:5.0f} ph {phase} arc {x/1000:6.1f} h {h:6.0f} vh {vh:6.0f} vz {vz:5.0f} pitch {math.degrees(th):5.1f} T {T:5.1f}")
        if math.hypot(vh,vz)<=VEND: return f"END t {t:.0f} arc {x/1000:.1f} km h {h:.0f} vh {vh:.0f} vz {vz:.0f} pitch {mn:.0f}..{mx:.0f}"
        if h<0: return f"CRASH t {t:.0f}"
    return "TIMEOUT"
for a in [285,330,400]:
    print(a, sim(a*1000.0, log=(len(sys.argv)>1 and int(sys.argv[1])==a)))
