import random

from eccfrog import scalar_mult,point_add,curve
import sys

def toHex(val,Gv):
	(x,y) = Gv
	x=hex(x)
	y=hex(y)
	print (f"({val}G) in hex: {x},{y})\n")

val=1
if (len(sys.argv)>1):
  val=int(sys.argv[1])

print ("ECCFrog512CK2")
n=1
G = scalar_mult(n,curve.g)
print (f"Point {n}G: {G}")
toHex(n,G)

n=2
G2 = scalar_mult(n,curve.g)
print (f"Point {n}G: {G2}")
toHex(n,G2)

n=3
G3 = scalar_mult(n,curve.g)
print (f"Point {n}G: {G3}")
toHex(n,G3)


G_G = point_add(G,G)
print (f"\nG+G: {G_G}")
G_2G = point_add(G,G2)
print (f"\nG+2G: {G_2G}")

Gv = scalar_mult(val,curve.g)
print (f"\nPoint {val}G: {Gv}")
toHex(val,Gv)

val=random.randint(0,curve.n)
Gv = scalar_mult(val,curve.g)
print (f"\nRandom Point {val}G: {Gv}")
