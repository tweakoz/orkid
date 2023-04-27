#!/usr/bin/env python3

from orkengine.core import *
import numpy as np


def do_test(name,matrix):

    print( "## %s ##############################" % name)

    A = matrix
    B = np.asarray(A, dtype= 'f8').reshape(4,4)

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



A = mtx4.lookAt(vec3(0,0,0),vec3(0,0,1),vec3(0,1,0))
col1 = A.getColumn(1)
col1.y = -col1.y
A.setColumn(1,col1)
B = mtx4.lookAt(vec3(0,0,0),vec3(0,0,1),vec3(0,-1,0))
C = mtx4()
C.setRow(0,vec4(1,2,3,4))
C.setRow(1,vec4(5,6,7,8))
C.setRow(2,vec4(9,8,7,6))
C.setRow(3,vec4(5,4,3,2))


do_test("A", A)
do_test("B", B)
do_test("C", C)