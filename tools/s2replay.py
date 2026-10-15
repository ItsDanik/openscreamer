"""
Screamer 2: A RACE RECORDED FROM THE ORIGINAL (tools/s2dos.py) THROUGH THE PORT, frame by frame.

    python3 tools/s2replay.py <prefix> [OS_DIFF=1 ...]

<prefix>.frames has, a line a frame of the original's, the race's clock, the step and the raw
time, the six controls as the game saw them, and every car's place, heading, speed.  The
port is given the same steps and controls (OS_S2SCRIPT) from the same clock (OS_S2CLOCK) and
its cars are printed beside the original's.  EACH=from,to prints those frames one by one.
"""
import sys, subprocess, os, math
S=os.environ.get('S2_SCRATCH','/tmp')
pre=sys.argv[1]
rows=[[float(x) for x in l.split()] for l in open(pre+'.frames')]
# the race = from the last time the clock went back
start=max(i for i in range(1,len(rows)) if rows[i][1] < rows[i-1][1]) if any(rows[i][1] < rows[i-1][1] for i in range(1,len(rows))) else 0
race=rows[start:]
ncars=(len(race[0])-10)//9
with open(pre+'.script','w') as f:
    for r in race[1:]: f.write("%.9g %.9g %d %d %d %d\n"%(r[2],r[3],r[4]>0,r[5]>0,r[6]>0,r[7]>0))
env=dict(os.environ,OPENSCREAMER_HOME=S+'/home',SDL_VIDEODRIVER='dummy',OS_TRACK='1',OS_S2CARS=str(ncars),OS_S2HOLD='1',OS_S2PHYS='1',OS_S2SCRIPT=pre+'.script',OS_S2CLOCK="%.9g"%race[0][1])
env.update(dict(a.split('=',1) for a in sys.argv[2:]))
out=subprocess.run(['/home/danik/openscreamer/port/build/openscreamer','-game','2'],env=env,capture_output=True,text=True,cwd='/home/danik/openscreamer').stdout
port=[[ [float(v) for v in part.split()] for part in l.split('|')[1:]] for l in out.splitlines() if l.startswith('F ')]
print("race frames read from the original:",len(race)-1,"| replayed in the port:",len(port),"| cars",ncars)
worst=0
for i,(r,p) in enumerate(zip(race[1:],port)):
    line=[]
    for k in range(ncars):
        o=r[10+k*9:19+k*9]   # x h z pitch head lean speed slip steer
        dx=p[k][0]-o[0]; dz=p[k][2]-o[2]; dh=(p[k][3]-o[4]+math.pi)%(2*math.pi)-math.pi
        worst=max(worst,abs(dx),abs(dz)) if k==0 else worst
        if k==0: line.append("me: original %7d %7d head %6.3f %5.1f km/h | port %7d %7d head %6.3f %5.1f | apart %5d %5d"%(o[0],o[2],o[4],o[6],p[0][0],p[0][2],p[0][3],p[0][4],dx,dz))
        else: line.append("car%d apart %d %d"%(k,dx,dz))
    if (i%8==0 and not os.environ.get("EACH")) or i==len(port)-1 or (os.environ.get("EACH") and int(os.environ["EACH"].split(",")[0])<=i<=int(os.environ["EACH"].split(",")[1])): print("f %3d clock %5.2f keys %d%d%d%d  "%(i,r[1],r[4]>0,r[5]>0,r[6]>0,r[7]>0)+" ; ".join(line))
