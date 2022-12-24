/*

    auto model = gpurec->_char_modelasset->getSharedModel();
    auto& skel = model->skeleton();
    if (0) { // fmod(time, 10) < 5) {

      int ji_lshoulder     = skel.jointIndex("mixamorig.LeftShoulder");
      auto lshoulder_base  = localpose._concat_matrices[ji_lshoulder];
      auto lshoulder_basei = lshoulder_base.inverse();

      fmtx4 rotmtx;
      rotmtx.setRotateY((sinf(time * 5) * 7.5) * DTOR);
      rotmtx = lshoulder_basei * rotmtx * lshoulder_base;

      //gpurec->_char_applicatorL->apply([&](int index) {
//        auto& ci = localpose._concat_matrices[index];
  //      ci       = (rotmtx * ci);
    //  });


*/