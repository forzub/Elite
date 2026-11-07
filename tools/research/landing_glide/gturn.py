import math
MU=4.9048695e12; R=1737400.0+4108.0
def fly(b, h=82707.0, vh=1638.98, vz=0.0, vend=60.0, dt=0.1):
    x=0.0; t=0.0
    while True:
        r=R+h; g=MU/r/r; v=math.hypot(vh,vz)
        if v<=vend or h<0 or t>3000: return x,h,t,math.degrees(math.atan2(vz,vh))
        ax=-b*vh/v; az=-b*vz/v-g+vh*vh/r
        vh+=ax*dt; vz+=az*dt; x+=vh*dt; h+=vz*dt; t+=dt
for b in [2,3,4,5,6,8,10,15,20]:
    x,h,t,gam=fly(b); print(f"b {b:5.1f}: arc {x/1000:7.1f} km, end h {h:8.0f} m, {t:6.1f} s, path {gam:6.1f} deg")
