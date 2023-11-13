#!/usr/bin/env python3
#########################################
import os, time
import openai
from obt import command, path, deco

deco = deco.Deco()
#########################################

API_KEY = os.environ['OPENAI_API_KEY']
MODEL = "gpt-4-1106-preview"
#MODEL = "gpt-4"

#########################################

client = openai.OpenAI(api_key=API_KEY)
print(client)

#########################################

def upload_files(file_paths):
  file_ids = []
  ####################
  # aggregate source files
  ####################
  aggregated = ""
  for file_path in file_paths:
    path_as_str = str(file_path)
    text = open(path_as_str, "r").read()
    aggregated += "// BEGIN SOURCE FILE: " + path_as_str + "\n"
    aggregated += text
    aggregated += "// END SOURCE FILE: " + path_as_str + "\n"
  ####################
  # write aggregated
  ####################
  out_path = path.temp()/"orkid_aggregated.h"
  open(str(out_path), "w").write(aggregated)
  ####################
  print(deco.yellow("uploading to context:  file<%s>"%out_path))
  # TODO - upload file without chatgpt being able 
  # to see the absolute path of the file
  file = client.files.create(
    file=open(str(out_path), "rb"),
    purpose='assistants'
  )
  file_ids.append(file.id)
  return file_ids

#########################################
# Upload your files
#########################################

core_dir = path.orkid()/"ork.core"
core_inc_dir = core_dir/"inc"
core_base_dir = core_inc_dir/"ork"
core_kern_inc = core_base_dir/"kernel"
core_util_inc = core_base_dir/"util"
core_appl_dir = core_base_dir/"application"
core_asst_dir = core_base_dir/"asset"
core_dflow_dir = core_base_dir/"dataflow"
core_rtti_dir = core_base_dir/"rtti"
core_refl_dir = core_base_dir/"reflect"
core_prop_dir = core_refl_dir/"properties"
core_tests_dir = core_dir/"tests"
#
lev2_dir = path.orkid()/"ork.lev2"
lev2_inc_dir = lev2_dir/"inc"
lev2_bas_dir = lev2_inc_dir/"ork"/"lev2"
lev2_gfx_inc = lev2_bas_dir/"gfx"
#
file_paths  = [core_base_dir/"orkconfig.h"] 
file_paths  = [core_base_dir/"orktypes.h"] 
file_paths += [core_base_dir/"orkstd.h"] 
file_paths += [core_base_dir/"file"/"efileenum.h"] 
file_paths += [core_base_dir/"file"/"file.h"] 
file_paths += [core_base_dir/"file"/"fileenv.h"] 
file_paths += [core_base_dir/"file"/"filedev.h"] 
file_paths += [core_base_dir/"file"/"filestd.h"] 
file_paths += [core_base_dir/"file"/"filedevcontext.h"] 
file_paths += [core_base_dir/"file"/"path.h"] 
file_paths += [core_base_dir/"file"/"chunkfile.h"] 
file_paths += [core_base_dir/"file"/"chunkfile.inl"] 
#
file_paths += [core_appl_dir/"application.h"] 
#
file_paths += [core_dflow_dir/"enum.h"] 
file_paths += [core_dflow_dir/"dataflow.h"] 
file_paths += [core_dflow_dir/"module.h"] 
file_paths += [core_dflow_dir/"plug_data.h"] 
file_paths += [core_dflow_dir/"plug_inst.h"] 
file_paths += [core_dflow_dir/"scheduler.h"] 
#
file_paths += [core_rtti_dir/"RTTI.h"] 
file_paths += [core_rtti_dir/"RTTIData.h"] 
file_paths += [core_rtti_dir/"RTTIX.inl"] 
file_paths += [core_rtti_dir/"ICastable.h"] 
file_paths += [core_rtti_dir/"Class.h"] 
file_paths += [core_rtti_dir/"Category.h"] 
#
file_paths += [core_refl_dir/"Description.h"] 
file_paths += [core_refl_dir/"ISerializer.h"] 
file_paths += [core_refl_dir/"Serializable.h"] 
file_paths += [core_refl_dir/"IDeserializer.h"] 
file_paths += [core_refl_dir/"IDeserializer.inl"] 
file_paths += [core_refl_dir/"Serialize.h"] 
file_paths += [core_refl_dir/"types.h"] 
file_paths += [core_refl_dir/"Function.h"] 
file_paths += [core_refl_dir/"Functor.h"] 
#
file_paths += [core_prop_dir/"DirectEnum.h"] 
file_paths += [core_prop_dir/"DirectObject.h"] 
file_paths += [core_prop_dir/"DirectObjectMap.h"] 
file_paths += [core_prop_dir/"DirectObjectVector.h"] 
file_paths += [core_prop_dir/"DirectTyped.h"] 
file_paths += [core_prop_dir/"DirectTypedMap.h"] 
file_paths += [core_prop_dir/"DirectTypedArray.h"] 
file_paths += [core_prop_dir/"DirectTypedVector.h"] 
#
file_paths += [core_prop_dir/"codec.h"] 
file_paths += [core_prop_dir/"codec.inl"] 
file_paths += [core_prop_dir/"IArray.h"] 
file_paths += [core_prop_dir/"IMap.h"] 
file_paths += [core_prop_dir/"IObject.h"] 
file_paths += [core_prop_dir/"IObjectArray.h"] 
file_paths += [core_prop_dir/"IObjectMap.h"] 
file_paths += [core_prop_dir/"ITyped.h"] 
file_paths += [core_prop_dir/"ITypedArray.h"] 
file_paths += [core_prop_dir/"ITypedMap.h"] 
file_paths += [core_prop_dir/"LambdaTyped.inl"] 
file_paths += [core_prop_dir/"ObjectProperty.h"] 
file_paths += [core_prop_dir/"register.h"] 
file_paths += [core_prop_dir/"registerX.inl"] 
#
file_paths += [core_kern_inc/"netpacket.inl"] 
file_paths += [core_kern_inc/"datacache.h"] 
file_paths += [core_kern_inc/"datablock.h"] 
file_paths += [core_kern_inc/"svariant.h"] 
file_paths += [core_kern_inc/"varmap.inl"] 
file_paths += [core_kern_inc/"opq.h"] 
file_paths += [core_kern_inc/"atomic.h"] 
file_paths += [core_kern_inc/"mutex.h"] 
file_paths += [core_kern_inc/"semaphore.h"] 
file_paths += [core_kern_inc/"thread.h"] 
file_paths += [core_kern_inc/"treeops.inl"] 
file_paths += [core_util_inc/"crc.h"] 
file_paths += [core_util_inc/"scanner.h"] 
file_paths += [core_util_inc/"parser.h"] 
file_paths += [core_kern_inc/"string"/"string.h"] 
#
file_paths += [core_tests_dir/"choiceman.cpp"]
file_paths += [core_tests_dir/"cmatrix4.cpp"]
file_paths += [core_tests_dir/"crcstring.cpp"]
file_paths += [core_tests_dir/"cquaternion.cpp"]
file_paths += [core_tests_dir/"cvector2.cpp"]
file_paths += [core_tests_dir/"cvector3.cpp"]
file_paths += [core_tests_dir/"cvector4.cpp"]
file_paths += [core_tests_dir/"dataflow.cpp"]
file_paths += [core_tests_dir/"file.cpp"]
file_paths += [core_tests_dir/"fixedstring.cpp"]
file_paths += [core_tests_dir/"fsm.cpp"]
file_paths += [core_tests_dir/"future.cpp"]
file_paths += [core_tests_dir/"netpacket_serdes.cpp"]
file_paths += [core_tests_dir/"object.cpp"]
file_paths += [core_tests_dir/"opq.cpp"]
file_paths += [core_tests_dir/"parser_lang.inl"]
file_paths += [core_tests_dir/"parser_1.cpp"]
file_paths += [core_tests_dir/"parser_2.cpp"]
file_paths += [core_tests_dir/"parser_3.cpp"]
file_paths += [core_tests_dir/"parser_3.cpp"]
file_paths += [core_tests_dir/"path.cpp"]
file_paths += [core_tests_dir/"reflection.cpp"]
file_paths += [core_tests_dir/"reflectionclasses.inl"]
file_paths += [core_tests_dir/"reflectionclasses.cpp"]
file_paths += [core_tests_dir/"ringbuffer.cpp"]
file_paths += [core_tests_dir/"serdes_array.cpp"]
file_paths += [core_tests_dir/"serdes_asset.cpp"]
file_paths += [core_tests_dir/"serdes_binary.cpp"]
file_paths += [core_tests_dir/"serdes_enum.cpp"]
file_paths += [core_tests_dir/"serdes_json.cpp"]
file_paths += [core_tests_dir/"serdes_leaf.cpp"]
file_paths += [core_tests_dir/"serdes_map.cpp"]
file_paths += [core_tests_dir/"serdes_math.cpp"]
file_paths += [core_tests_dir/"serdes_uuid.cpp"]
file_paths += [core_tests_dir/"serdes_vector.cpp"]
file_paths += [core_tests_dir/"slashnode.cpp"]
file_paths += [core_tests_dir/"svariant.cpp"]
#
file_paths += [lev2_bas_dir/"init.h"] 
file_paths += [lev2_bas_dir/"lev2_types.h"] 
file_paths += [lev2_bas_dir/"lev2_asset.h"] 
file_paths += [lev2_bas_dir/"ezapp.h"] 
file_paths += [lev2_gfx_inc/"ctxbase.h"] 
file_paths += [lev2_gfx_inc/"fx_pipeline.h"] 
file_paths += [lev2_gfx_inc/"fxi.h"] 
file_paths += [lev2_gfx_inc/"gbi.h"] 
file_paths += [lev2_gfx_inc/"fbi.h"] 
file_paths += [lev2_gfx_inc/"mtxi.h"] 
file_paths += [lev2_gfx_inc/"dwi.h"] 
file_paths += [lev2_gfx_inc/"imi.h"] 
file_paths += [lev2_gfx_inc/"ci.h"] 
file_paths += [lev2_gfx_inc/"txi.h"] 
#
file_paths += [lev2_gfx_inc/"gfxenv.h"] 
file_paths += [lev2_gfx_inc/"gfxenv_enum.h"] 
file_paths += [lev2_gfx_inc/"gfxprimitives.h"] 
file_paths += [lev2_gfx_inc/"gfxrasterstate.h"] 
file_paths += [lev2_gfx_inc/"gfxvtxbuf.h"] 
file_paths += [lev2_gfx_inc/"image.h"] 
file_paths += [lev2_gfx_inc/"shadman.h"] 
file_paths += [lev2_gfx_inc/"texman.h"] 
file_paths += [lev2_gfx_inc/"rtgroup.h"] 
file_paths += [lev2_gfx_inc/"gfxmaterial.h"] 
file_paths += [lev2_gfx_inc/"material_freestyle.h"] 
file_paths += [lev2_gfx_inc/"material_pbr.inl"] 
file_paths += [lev2_gfx_inc/"shadlang.h"] 
file_paths += [lev2_gfx_inc/"shadlang_nodes.h"] 
file_paths += [lev2_gfx_inc/"gfxmodel.h"] 
file_paths += [lev2_gfx_inc/"gfxanim.h"] 
file_paths += [lev2_gfx_inc/"ikchain.h"] 
#
file_paths += [lev2_gfx_inc/"camera"/"cameradata.h"] 
file_paths += [lev2_gfx_inc/"camera"/"uicam.h"] 
file_paths += [lev2_gfx_inc/"lighting"/"gfx_lighting.h"] 
#
file_paths += [lev2_gfx_inc/"particle"/"drawable_data.h"] 
file_paths += [lev2_gfx_inc/"particle"/"modular_emitters.h"] 
file_paths += [lev2_gfx_inc/"particle"/"modular_forces.h"] 
file_paths += [lev2_gfx_inc/"particle"/"modular_particles2.h"] 
file_paths += [lev2_gfx_inc/"particle"/"modular_renderers.h"] 
file_paths += [lev2_gfx_inc/"particle"/"particle.h"] 
file_paths += [lev2_gfx_inc/"particle"/"particle.hpp"] 
#
file_paths += [lev2_gfx_inc/"renderer"/"renderer.h"] 
file_paths += [lev2_gfx_inc/"renderer"/"renderable.h"] 
file_paths += [lev2_gfx_inc/"renderer"/"rendercontext.h"] 
file_paths += [lev2_gfx_inc/"renderer"/"renderqueue.h"] 
file_paths += [lev2_gfx_inc/"renderer"/"renderer_enum.h"] 
file_paths += [lev2_gfx_inc/"renderer"/"drawable.h"] 
file_paths += [lev2_gfx_inc/"renderer"/"compositor.h"] 
file_paths += [lev2_gfx_inc/"renderer"/"NodeCompositor"/"NodeCompositor.h"] 
file_paths += [lev2_gfx_inc/"renderer"/"NodeCompositor"/"pbr_common.h"] 
file_paths += [lev2_gfx_inc/"renderer"/"NodeCompositor"/"pbr_light_processor_cpu.h"] 
file_paths += [lev2_gfx_inc/"renderer"/"NodeCompositor"/"pbr_light_processor_simple.h"] 
file_paths += [lev2_gfx_inc/"renderer"/"NodeCompositor"/"pbr_node_deferred.h"] 
file_paths += [lev2_gfx_inc/"renderer"/"NodeCompositor"/"pbr_node_forward.h"] 
file_paths += [lev2_gfx_inc/"scenegraph"/"scenegraph.h"] 
file_paths += [lev2_gfx_inc/"scenegraph"/"sgnode_grid.h"] 
file_paths += [lev2_gfx_inc/"scenegraph"/"sgnode_billboard.h"] 
file_paths += [lev2_gfx_inc/"scenegraph"/"sgnode_groundplane.h"] 
file_paths += [lev2_gfx_inc/"scenegraph"/"sgnode_particles.h"] 

file_ids = upload_files(file_paths)

#########################################
# Create an assistant with knowledge retrieval tool
#########################################

assistant = client.beta.assistants.create(
  instructions="You are a domain expert of a c++ game engine named Orkid. When asked questions about this engine, respond with the approprate amount of detail to the question. stored in the assistant file context is a bunch of source files, each demarked with // BEGIN SOURCE FILE: and // END SOURCE FILE: so can determine which files hold which source..",
  model=MODEL,
  tools=[{"type": "retrieval"}],
  #tools=[{"type": "code_interpreter"}],
  file_ids=file_ids
)

thread = client.beta.threads.create()

#########################################
# Function to ask questions to the assistant
#########################################

def ask_question(question):
  message = client.beta.threads.messages.create(
    thread_id = thread.id,
    file_ids=file_ids,
    role="user",
    content=question
  )
  run = client.beta.threads.runs.create(
    thread_id=thread.id,
    assistant_id=assistant.id
  )
  print("waiting for reponse.")
  keep_waiting = True
  counter = 0
  while keep_waiting:
    run_status = client.beta.threads.runs.retrieve(
      thread_id = thread.id,
      run_id = run.id
    )
    if run_status.status == "completed":
      keep_waiting = False
    else:
      counter+=1
      # print spinning wheel
      if counter%4 == 0:
        print("\r|", end = '')
      elif counter%4 == 1:
        print("\r/", end = '')
      elif counter%4 == 2:
        print("\r-", end = '')
      elif counter%4 == 3:
        print("\r\\", end = '')
      time.sleep(1)
  messages = client.beta.threads.messages.list(
    thread_id=thread.id,
  )
  answer = []
  for message in messages:
    role = message.role
    content = message.content[0].text.value
    answer += [content]
  print("") 
  return answer[0]

#########################################
# repl loop
#########################################

keep_going = True
while(keep_going):
  q = input(">>> ")
  if q=="done":
    keep_going = False
  else:
    answer = ask_question(q)
    print("<<<" + deco.key(answer))

