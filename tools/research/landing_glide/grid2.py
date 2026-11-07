import math
MU=4.9048695e12; R=1737400.0+4108.0
def predict(state, p1, T1, ts, p2, T2, vend, dt=1.0):
    x,h,vh,vz=state; t=0.0; P1=math.radians(p1); P2=math.radians(p2)
    while t<2500:
        r=R+h; defi=MU/r/r - vh*vh/r
        if math.hypot(vh,vz)<=vend or h<-500: break
        P,T=(P1,T1) if t<ts else (P2,T2)
        # nose pitch P above the horizon, reverse thrust along -nose
        ax=-T*math.cos(P); az=-T*math.sin(P)-defi
        vh+=ax*dt; vz+=az*dt; x-=vh*dt; h+=vz*dt; t+=dt
    return x,h,vz,t
import sys
arc0=float(sys.argv[1])*1000 if len(sys.argv)>1 else 285512.0
st=(arc0,82707.0,1638.98,0.0)
res=[]
for p1 in [0,5,10,15,20]:
  for T1x in range(0,13):
    T1=T1x*0.5
    for ts in range(0,500,20):
      for p2 in [-10,-15,-20,-25]:
        for T2x in range(2,13):
            T2=T2x*2.5
            x,h,vz,t=predict(st,p1,T1,ts,p2,T2,450.0)
            cost=abs(x-15000)/1000+abs(h-8000)/1000+max(0,-vz-350)/10
            res.append((cost,p1,T1,ts,p2,T2,x/1000,h,vz,t))
res.sort()
for r in res[:8]: print("cost %.2f p1 %d T1 %.1f ts %d p2 %d T2 %.1f -> arc %.1f h %.0f vz %.0f t %.0f"%r)
