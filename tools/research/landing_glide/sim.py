import math, sys
MU=4.9048695e12; R=1737400.0; GROUND=4108.0
MAIN=73.5; REV=73.5; RCS=0.0; PITCHRATE=math.radians(float(sys.argv[2]) if len(sys.argv)>2 else 20)
HH=1500.0; HARC=1500.0; ENDARC=0.5*HARC; VEND=60.0
def run(variant, arc0=285512.0, h0=82707.0, v0=1638.98, dt=0.05, log=False):
    x=arc0; h=h0; vh=v0; vz=0.0; th=0.0; t=0.0
    st={}
    worst=0
    while t<900:
        r=R+GROUND+h; g=MU/r/r
        a=variant(st,x,h,vh,vz,g,r)
        aH,aZ=a  # aH: + toward the site
        # pilot: nose along the path (pitch clamp), tilt for the part across it
        sp=math.hypot(vh,vz)
        pth=math.atan2(vz,max(vh,1e-6)) if sp>30 else 0.0
        pth=max(math.radians(-25),min(math.radians(15),pth))
        n0=(math.cos(pth),math.sin(pth))
        aPar=aH*n0[0]+aZ*n0[1]; perp=(aH-n0[0]*aPar, aZ-n0[1]*aPar); pl=math.hypot(*perp)
        nose=pth
        if pl-0.8*RCS>0 and pl>1e-6:
            tilt=min(math.radians(35),math.atan2(pl-0.8*RCS,max(abs(aPar),0.5)))
            push=aPar>0.2*pl+0.5
            d=(perp[0]/pl,perp[1]/pl)
            s=math.sin(tilt) if push else -math.sin(tilt)
            nv=(n0[0]*math.cos(tilt)+d[0]*s, n0[1]*math.cos(tilt)+d[1]*s)
            nose=math.atan2(nv[1],nv[0])
        dth=max(-PITCHRATE*dt,min(PITCHRATE*dt,nose-th)); th+=dth
        f=(math.cos(th),math.sin(th))
        al=math.cos(th-nose); lo=math.cos(math.radians(20)); hi=math.cos(math.radians(4))
        tt=max(0,min(1,(al-lo)/(hi-lo))); gate=tt*tt*(3-2*tt)
        aN=aH*f[0]+aZ*f[1]
        cmd=0.0
        if abs(aN)>0.5*RCS: cmd=max(-1,min(1,aN/MAIN if aN>=0 else aN/REV))*gate
        T=cmd*(MAIN if cmd>=0 else REV)
        ax=f[0]*T; az=f[1]*T-g+vh*vh/r
        vh+=ax*dt; vz+=az*dt; x-=vh*dt; h+=vz*dt; t+=dt
        worst=max(worst,abs(math.degrees(th)))
        if log and int(t/dt)%int(20/dt)==0:
            print(f"t {t:6.1f} arc {x/1000:7.2f} km h {h:8.0f} vh {vh:7.1f} vz {vz:7.1f} pitch {math.degrees(th):6.1f} cmd {cmd:5.2f} want ({aH:6.2f},{aZ:6.2f})")
        if (x<2*HARC and math.hypot(vh,vz)<max(80,VEND*1.33) and h<1.7*HH) or (h<600 and math.hypot(vh,vz)<450):
            return f"HANDOVER t {t:.1f} arc {x:.0f} m h {h:.0f} v {math.hypot(vh,vz):.1f} (vh {vh:.1f} vz {vz:.1f}) pitch {math.degrees(th):.1f} max|pitch| {worst:.1f}"
        if h<0: return f"CRASH t {t:.1f} arc {x:.0f} v {vh:.1f}/{vz:.1f}"
    return "TIMEOUT"
