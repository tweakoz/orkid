#!/usr/bin/env python3

from orkengine.core import *
import numpy as np
import math


def do_test(name,matrix):

    print( "## %s ##############################" % name)

    A = matrix
    B = np.array(A, dtype= 'f8').reshape(4,4)

    print("Orkid Matrix (OM): ", A.dump4x4cn() )
    print("NumPy Matrix (NM): ", str(B).replace("\n", " "))

    print("A==B", str(B==A).replace("\n", " "))

    #################################
    # A
    #################################

    a_ev = A.eigenvectors
    print("OM.determinant: ", A.determinant)
    print("OM.eigenvalues: ", A.eigenvalues)
    print("OM.eigenvectors: ", a_ev.dump4x4cn() )

    ev0 = a_ev.getColumn(0)
    ev1 = a_ev.getColumn(1)
    ev2 = a_ev.getColumn(2)
    ev3 = a_ev.getColumn(3)

    print( "OM.ev0: ", ev0,  ev0.transform(A) )
    print( "OM.ev1: ", ev1,  ev1.transform(A) )
    print( "OM.ev2: ", ev2,  ev2.transform(A) )
    print( "OM.ev3: ", ev3,  ev3.transform(A) )

    #################################
    # B
    #################################

    eigenvalues, eigenvectors = np.linalg.eig(B)

    print("NM.determinant: ", np.linalg.det(B))
    print("NM.eigenvalues: ", eigenvalues)
    print("NM.eigenvectors: ", str(eigenvectors).replace("\n", " "))


#######################################
A = dmtx4.lookAt(dvec3(0,0,0),dvec3(0,0,1),dvec3(0,1,0))
col1 = A.getColumn(1)
col1.y = -col1.y
A.setColumn(1,col1)
#######################################
B = dmtx4.lookAt(dvec3(0,0,0),dvec3(0,0,1),dvec3(0,-1,0))
#######################################
C = dmtx4(dquat(dvec3(1,1,1).normalized,math.pi))
#######################################
D = dmtx4()
D.setRow(0,dvec4(1,2,3,4))
D.setRow(1,dvec4(5,6,7,8))
D.setRow(2,dvec4(9,8,7,6))
D.setRow(3,dvec4(5,4,3,2))
#######################################
do_test("A", A)
do_test("B", B)
do_test("C", C)
do_test("D", D)