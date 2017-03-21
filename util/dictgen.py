import sys

PREV=6

poss = dict()

def getstate(pref):
  if len(pref) > 0 and pref[-1] == "\n":
    return None
  return pref[max(0, len(pref) - PREV):]

for line in sys.stdin:
  lin = line.rstrip("\r\n") + "\n"
  for p in range(len(lin)):
    prv = getstate(lin[0:p])
    nxt = lin[p:p+1]
    if prv not in poss:
      poss[prv] = set()
    poss[prv].add((nxt, getstate(lin[0:p+1])))

snames = dict()
for prv in poss:
  snames[prv] = [prv]

while True:
  nposs = dict()
  for prv in poss:
    if len(poss[prv]) <= 1:
      continue
    lst = sorted([x for x in poss[prv]])
    nlst = []
    for (nxt, st) in lst:
      nxt = nxt.rstrip("\n")
      while st is not None and len(poss[st]) == 1:
        sing = [x for x in poss[st]][0]
        nxt += sing[0]
        st = sing[1]
      nlst.append((nxt, st))
    nposs[prv] = nlst
  poss = nposs

  mm = dict()
  dups = 0
  for k in poss:
    r = "%r" % (poss[k],)
    if r not in mm:
      mm[r] = []
    mm[r] = sorted(mm[r] + [k])

  remap = dict()
  num = 0
  for kk in sorted(mm, key=lambda x: mm[x]):
    dups += len(mm[kk]) - 1
    for ll in mm[kk]:
      remap[ll] = num
    num += 1

  done = set()
  nposs = dict()
  nsnames = dict()
  for prv in poss:
    if remap[prv] not in nsnames:
      nsnames[remap[prv]] = snames[prv]
    else:
      nsnames[remap[prv]] = sorted(nsnames[remap[prv]] + snames[prv])
    if remap[prv] in done:
      continue
    done.add(remap[prv])
    lst = sorted([x for x in poss[prv]])
    nlst = []
    for (nxt, st) in lst:
      nlst.append((nxt, None if st is None else remap[st]))
    nposs[remap[prv]] = nlst

  poss = nposs
  snames = nsnames
  if dups == 0:
    break

for prv in sorted(poss.keys()):
  sys.stdout.write("s%i = " % prv + " | ".join(["\"%s\"" % x[0] + (" s%i" % x[1] if x[1] is not None else "") for x in poss[prv]]) + "; # %s\n" % ','.join(["<init>" if x == "" else x for x in snames[prv]]))

sys.stdout.write("main = s0 (\" \" s0)+;\n")
