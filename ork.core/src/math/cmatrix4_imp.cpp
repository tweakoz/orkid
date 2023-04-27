////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/math/cmatrix4.h>
#include <ork/math/cmatrix4.hpp>
#include <ork/kernel/string/string.h>
#include <openblas/lapacke.h>

namespace ork {
template <> const EPropType PropType<fmtx4>::meType   = EPROPTYPE_MAT44REAL;
template <> const char* PropType<fmtx4>::mstrTypeName = "MAT44REAL";
template <> void PropType<fmtx4>::ToString(const fmtx4& Value, PropTypeString& tstr) {
  const fmtx4& v = Value;

  std::string result;
  for (int i = 0; i < 15; i++)
    result += CreateFormattedString("%g ", F32(v.elemXY(i / 4,i % 4)));
  result += CreateFormattedString("%g", F32(v.elemXY(3,3)));
  tstr.format("%s", result.c_str());
}

template <> fmtx4 PropType<fmtx4>::FromString(const PropTypeString& String) {
  float m[4][4];
  sscanf(
      String.c_str(),
      "%g %g %g %g %g %g %g %g %g %g %g %g %g %g %g %g",
      &m[0][0],
      &m[0][1],
      &m[0][2],
      &m[0][3],
      &m[1][0],
      &m[1][1],
      &m[1][2],
      &m[1][3],
      &m[2][0],
      &m[2][1],
      &m[2][2],
      &m[2][3],
      &m[3][0],
      &m[3][1],
      &m[3][2],
      &m[3][3]);
  fmtx4 result;
  for (int i = 0; i < 16; i++)
    result.setElemXY(i / 4,i % 4, m[i / 4][i % 4]);
  return result;
}

///////////////////////////////////////////////////////////////////////////////

template <> Vector4<double> Matrix44<double>::eigenvalues() const {

  int n = 4;
  char jobvl = 'N', jobvr = 'N';
  std::vector<double> wr(n), wi(n);
  std::vector<double> vl(n * n), vr(n * n);
  int lda = n, ldvl = n, ldvr = n;
  auto this_data = (double*) asArray(); // crappy c api's
  int status = LAPACKE_dgeev( LAPACK_ROW_MAJOR, //
                              jobvl, jobvr, n, //
                              this_data, lda, //
                              wr.data(), wi.data(), vl.data(), //
                              ldvl, vr.data(), ldvr);

  Vector4<double> rval(0,0,0,0);
  if(status==0){
    rval.x = wr[0];
    rval.y = wr[1];
    rval.z = wr[2];
    rval.w = wr[3];
  }
  return rval;

}

template <> Vector4<float> Matrix44<float>::eigenvalues() const {

  int n = 4;
  char jobvl = 'N', jobvr = 'N';
  std::vector<float> wr(n), wi(n);
  std::vector<float> vl(n * n), vr(n * n);
  int lda = n, ldvl = n, ldvr = n;
  auto this_data = (float*) asArray(); // crappy c api's
  int status = LAPACKE_sgeev( LAPACK_ROW_MAJOR, //
                              jobvl, jobvr, n, //
                              this_data, lda, //
                              wr.data(), wi.data(), vl.data(), //
                              ldvl, vr.data(), ldvr);
  Vector4<float> rval(0,0,0,0);
  if(status==0){
    rval.x = wr[0];
    rval.y = wr[1];
    rval.z = wr[2];
    rval.w = wr[3];
  }
  return rval;

}

template <> Matrix44<double> Matrix44<double>::eigenvectors() const {

  int n = 4;
  char jobvl = 'N', jobvr = 'V';
  std::vector<double> wr(n), wi(n);
  std::vector<double> vl(n * n), vr(n * n);
  int lda = n, ldvl = n, ldvr = n;
  auto this_data = (double*) asArray(); // crappy c api's
  int status = LAPACKE_dgeev( LAPACK_ROW_MAJOR, //
                              jobvl, jobvr, n, //
                              this_data, lda, //
                              wr.data(), wi.data(), vl.data(), //
                              ldvl, vr.data(), ldvr);
  Matrix44<double> rval;
  if(status==0){

    for( int i=0; i<4; i++ ){
      double norm = std::sqrt(vr[0+i]*vr[0+i] 
                            + vr[4+i]*vr[4+i] 
                            + vr[8+i]*vr[8+i] 
                            + vr[12+i]*vr[12+i]);
      for( int j=0; j<4; j++ ){
        rval.setElemXY(i,j,vr[i*4+j]/norm);

      }
    }
  }
  else{
    for( int i=0; i<4; i++ )
      for( int j=0; j<4; j++ )
        rval.setElemXY(i,j,0);
  }
  return rval;
}

template <> Matrix44<float> Matrix44<float>::eigenvectors() const {

  int n = 4;
  char jobvl = 'N', jobvr = 'V';
  std::vector<float> wr(n), wi(n);
  std::vector<float> vl(n * n), vr(n * n);
  int lda = n, ldvl = n, ldvr = n;
  auto this_data = (float*) asArray(); // crappy c api's
  int status = LAPACKE_sgeev( LAPACK_ROW_MAJOR, //
                              jobvl, jobvr, n, //
                              this_data, lda, //
                              wr.data(), wi.data(), vl.data(), //
                              ldvl, vr.data(), ldvr);
  Matrix44<float> rval;
  if(status==0){
    for( int i=0; i<4; i++ ){
      double norm = std::sqrt(vr[0+i]*vr[0+i] 
                            + vr[4+i]*vr[4+i] 
                            + vr[8+i]*vr[8+i] 
                            + vr[12+i]*vr[12+i]);
      for( int j=0; j<4; j++ ){
        rval.setElemXY(i,j,vr[i*4+j]/norm); 
      }
    }
  }
  else{
    for( int i=0; i<4; i++ )
      for( int j=0; j<4; j++ )
        rval.setElemXY(i,j,0);
  }
  return rval;

}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


template struct PropType<fmtx4>;
template struct Matrix44<float>; // explicit template instantiation
template struct Matrix44<double>; // explicit template instantiation

fmtx4 dmtx4_to_fmtx4(const dmtx4& in) {
  fmtx4 out;
  for (int i = 0; i < 9; i++)
    out.setElemXY(i / 4,i % 4, float(in.elemXY(i / 4,i % 4)));
  return out;
}

dmtx4 fmtx4_to_dmtx4(const fmtx4& in) {
  dmtx4 out;
  for (int i = 0; i < 9; i++)
    out.setElemXY(i / 4,i % 4, double(in.elemXY(i / 4,i % 4)));
  return out;
}

} // namespace ork
