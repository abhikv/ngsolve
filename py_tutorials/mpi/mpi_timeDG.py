from netgen.geom2d import unit_square
from ngsolve import *
import netgen.meshing as netgen

from ngsolve.ngstd import MPIManager
MPIManager.InitMPI()
rank = MPIManager.GetRank()
np = MPIManager.GetNP()

#mesh = Mesh (unit_square.GenerateMesh(maxh=0.1))
ngmesh = netgen.Mesh(dim=2)
ngmesh.Load("square.vol.gz")
mesh = Mesh(ngmesh)

fes = L2(mesh, order=4)

u = fes.TrialFunction()
v = fes.TestFunction()

b = CoefficientFunction( (y-0.5,0.5-x) )
bn = b*specialcf.normal(2)

ubnd = CoefficientFunction(0)

a = BilinearForm(fes)
a += SymbolicBFI (-u * b*grad(v))
a += SymbolicBFI ( bn*IfPos(bn, u, u.Other()) * (v-v.Other()), VOL, skeleton=True)
a += SymbolicBFI ( bn*IfPos(bn, u, ubnd) * v, BND, skeleton=True)
#a += SymbolicBFI (bn*IfPos(bn, u, u.Other(bnd=ubnd)) * v, element_boundary=True)

u = GridFunction(fes)
u.Set(exp (-40 * ( (x-0.7)*(x-0.7) + (y-0.7)*(y-0.7) )))

w = u.vec.CreateVector()

Draw (u, autoscale=False, sd=2)

t = 0
tau = 1e-3
tend = 10
count = 0

vtk_interval = int(0.02/tau);

with TaskManager():
    while t < tend:
        if rank==0:
            print("t = ", t)
        a.Apply (u.vec, w)
        fes.SolveM (rho=CoefficientFunction(1), vec=w)
        
        u.vec.data -= tau * w
        t += tau
        Redraw(blocking=True)

        #u.vec.Cumulate()
        if count%vtk_interval==0:
            vtk = VTKOutput(ma=mesh,coefs=[u],names=["sol"],filename="vtkout_p"+str(rank)+"_n"+str(int(count/vtk_interval)),subdivision=2)
            vtk.Do()
        count = count+1;
        # if rank==0:
        #     input("A??")
        MPIManager.Barrier()
