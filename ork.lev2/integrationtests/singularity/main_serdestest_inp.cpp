#include <ork/lev2/ezapp.h>
#include <ork/lev2/aud/singularity/krzdata.h>
#include <ork/lev2/aud/singularity/tx81z.h>
#include <ork/lev2/aud/singularity/alg_oscil.h>
#include <ork/lev2/aud/singularity/alg_amp.h>
#include <ork/kernel/string/string.h>
#include <ork/reflect/serialize/JsonDeserializer.h>
#include <ork/reflect/serialize/JsonSerializer.h>
#include <ork/file/file.h>
#include <ork/file/path.h>
#include <ork/kernel/environment.h>

using namespace ork;
using namespace ork::lev2;
using namespace ork::reflect::serdes;
using namespace ork::audio::singularity;

int main(int argc, char** argv, char** envp) {
  auto app = EzApp::get(argc, argv); // reflection init
  Environment env;
  env.init_from_envp(envp);

  OrkAssert(env.has("ORKID_WORKSPACE_DIR"));

  std::string orkdirstr;
  env.get("ORKID_WORKSPACE_DIR", orkdirstr);
  auto orkdir = file::Path(orkdirstr);

  auto inppath = orkdir               //
                 / "ork.lev2"         //
                 / "integrationtests" //
                 / "singularity"      //
                 / "serdes_test1.json";

  File jsonfile(inppath, EFM_READ);
  size_t length = 0;
  jsonfile.GetLength(length);
  std::string jsonstr;
  jsonstr.resize(length + 1);
  jsonfile.Read(jsonstr.data(), length);
  jsonstr.data()[length] = 0;

  // printf("%s\n", jsonstr.c_str());
  object_ptr_t instance_out;
  JsonDeserializer deser(jsonstr);
  deser.deserializeTop(instance_out);

  return 0;
}
